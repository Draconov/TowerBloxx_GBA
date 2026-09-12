from __future__ import annotations

from dataclasses import asdict
import hashlib
import json
import math
from pathlib import Path
import shutil
import struct
import zipfile
import zlib

from .io import CANONICAL_SHA256, validate_canonical_jar
from .manifest import sha256_bytes
from .m3g import Camera, Image2D, parse_m3g
from .m3g_geometry import GAME_MESH_USER_IDS, build_mesh_catalog, decode_image_rgba, resolve_mesh
from .m3g_render import (
    TextureRGBA,
    identity_matrix,
    post_rotate,
    post_translate,
    render_mesh_reference,
    runtime_camera,
)
from .resources import read_resource

_TARGET_WIDTH = 240
_TARGET_HEIGHT = 160
_BASE_FOV = 60.0
_NEAR = 10.0
_FAR = 10000.0
_REFERENCE_PITCH_DEGREES = 18.0
_REFERENCE_YAW_DEGREES = -35.0


def _write_json(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(value, ensure_ascii=False, sort_keys=True, indent=2) + "\n",
        encoding="utf-8",
        newline="\n",
    )


def _png_chunk(chunk_type: bytes, payload: bytes) -> bytes:
    checksum = zlib.crc32(chunk_type)
    checksum = zlib.crc32(payload, checksum) & 0xFFFFFFFF
    return struct.pack(">I", len(payload)) + chunk_type + payload + struct.pack(">I", checksum)


def encode_png_rgba(width: int, height: int, rgba: bytes) -> bytes:
    if width <= 0 or height <= 0:
        raise ValueError("PNG dimensions must be positive")
    if len(rgba) != width * height * 4:
        raise ValueError("PNG RGBA byte count does not match dimensions")
    scanlines = bytearray()
    stride = width * 4
    for y in range(height):
        scanlines.append(0)  # deterministic filter: None
        start = y * stride
        scanlines.extend(rgba[start : start + stride])
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    compressed = zlib.compress(bytes(scanlines), level=9)
    return (
        b"\x89PNG\r\n\x1a\n"
        + _png_chunk(b"IHDR", ihdr)
        + _png_chunk(b"IDAT", compressed)
        + _png_chunk(b"IEND", b"")
    )


def _bgr555(red: int, green: int, blue: int) -> int:
    return (red >> 3) | ((green >> 3) << 5) | ((blue >> 3) << 10)


def _bgr555_frame(rgba: bytes) -> bytes:
    output = bytearray()
    for offset in range(0, len(rgba), 4):
        red, green, blue, alpha = rgba[offset : offset + 4]
        value = 0 if alpha == 0 else _bgr555(red, green, blue)
        output.extend(struct.pack("<H", value))
    return bytes(output)


def _alpha_mask(rgba: bytes) -> bytes:
    result = bytearray((len(rgba) // 4 + 7) // 8)
    for pixel_index, offset in enumerate(range(0, len(rgba), 4)):
        if rgba[offset + 3]:
            result[pixel_index >> 3] |= 1 << (pixel_index & 7)
    return bytes(result)


def _indexed_derivative(rgba: bytes) -> dict[str, object]:
    opaque_colors: list[int] = []
    seen: set[int] = set()
    transparent_pixels = 0
    partial_alpha_pixels = 0
    for offset in range(0, len(rgba), 4):
        red, green, blue, alpha = rgba[offset : offset + 4]
        if alpha == 0:
            transparent_pixels += 1
            continue
        if alpha != 255:
            partial_alpha_pixels += 1
        color = _bgr555(red, green, blue)
        if color not in seen:
            seen.add(color)
            opaque_colors.append(color)

    # Palette index zero must remain transparent for GBA OBJ-style derivatives.
    palette_entries = 1 + len(opaque_colors)
    binary_alpha = partial_alpha_pixels == 0
    four_bpp = binary_alpha and palette_entries <= 16
    eight_bpp = binary_alpha and palette_entries <= 256
    if not eight_bpp:
        return {
            "unique_bgr555_colors": len(opaque_colors),
            "palette_entries": palette_entries,
            "transparent_pixels": transparent_pixels,
            "partial_alpha_pixels": partial_alpha_pixels,
            "four_bpp_lossless": False,
            "eight_bpp_lossless": False,
            "index_bpp": None,
            "palette": None,
            "indices": None,
        }

    color_to_index = {color: index + 1 for index, color in enumerate(opaque_colors)}
    indices = bytearray()
    for offset in range(0, len(rgba), 4):
        red, green, blue, alpha = rgba[offset : offset + 4]
        if alpha == 0:
            indices.append(0)
        else:
            indices.append(color_to_index[_bgr555(red, green, blue)])

    if four_bpp:
        packed = bytearray()
        for offset in range(0, len(indices), 2):
            low = indices[offset]
            high = indices[offset + 1] if offset + 1 < len(indices) else 0
            packed.append(low | (high << 4))
        index_bytes = bytes(packed)
        palette_size = 16
        index_bpp = 4
    else:
        index_bytes = bytes(indices)
        palette_size = 256
        index_bpp = 8

    palette_values = [0, *opaque_colors]
    palette_values.extend([0] * (palette_size - len(palette_values)))
    palette = b"".join(struct.pack("<H", value) for value in palette_values)
    return {
        "unique_bgr555_colors": len(opaque_colors),
        "palette_entries": palette_entries,
        "transparent_pixels": transparent_pixels,
        "partial_alpha_pixels": partial_alpha_pixels,
        "four_bpp_lossless": four_bpp,
        "eight_bpp_lossless": eight_bpp,
        "index_bpp": index_bpp,
        "palette": palette,
        "indices": index_bytes,
    }


def _reference_draw_transform(mesh, camera) -> tuple[float, ...]:
    minimum, maximum = mesh.aabb
    center = tuple((low + high) * 0.5 for low, high in zip(minimum, maximum))
    radius = math.sqrt(sum(((maximum[i] - minimum[i]) * 0.5) ** 2 for i in range(3)))
    half_fov = math.radians(camera.fov_y * 0.5)
    # Bounding-sphere framing remains stable across odd authoring proportions
    # such as the crane meshes while still using class n's GBA projection.
    distance = max(camera.near + radius + 1.0, radius / max(math.sin(half_fov), 0.05) * 1.18)

    transform = identity_matrix()
    transform = post_translate(transform, 0.0, 0.0, -distance)
    transform = post_rotate(transform, _REFERENCE_PITCH_DEGREES, 1.0, 0.0, 0.0)
    transform = post_rotate(transform, _REFERENCE_YAW_DEGREES, 0.0, 1.0, 0.0)
    transform = post_translate(transform, -center[0], -center[1], -center[2])
    return transform


def _scene_summary(scene) -> dict[str, object]:
    stored_cameras = [obj for obj in scene.objects[1:] if isinstance(obj, Camera)]
    stored_camera = stored_cameras[0] if stored_cameras else None
    camera = runtime_camera(_TARGET_WIDTH, _TARGET_HEIGHT)
    return {
        "header": asdict(scene.header),
        "sections": [
            {
                "offset": section.offset,
                "compression_scheme": section.compression_scheme,
                "total_length": section.total_length,
                "uncompressed_length": section.uncompressed_length,
                "object_bytes_length": len(section.object_bytes),
                "adler32": section.checksum,
            }
            for section in scene.sections
        ],
        "object_count": len(scene.chunks),
        "object_type_counts": scene.type_counts(),
        "world": {
            "object_index": scene.world.object_index,
            "user_id": scene.world.user_id,
            "children": list(scene.world.children),
            "active_camera": scene.world.active_camera,
            "background": scene.world.background,
        },
        "stored_authoring_camera": None if stored_camera is None else {
            "object_index": stored_camera.object_index,
            "user_id": stored_camera.user_id,
            "translation": list(stored_camera.translation),
            "orientation_angle": stored_camera.orientation_angle,
            "orientation_axis": list(stored_camera.orientation_axis),
            "projection_type": stored_camera.projection_type,
            "fovy": stored_camera.fovy,
            "aspect_ratio": stored_camera.aspect_ratio,
            "near": stored_camera.near,
            "far": stored_camera.far,
        },
        "java_runtime_camera": {
            "target_width": camera.width,
            "target_height": camera.height,
            "base_fov": camera.base_fov,
            "effective_fov_y": camera.fov_y,
            "effective_aspect_ratio": camera.aspect_ratio,
            "near": camera.near,
            "far": camera.far,
        },
        "java_mesh_wrapper": {
            "node_transform_ignored_by_immediate_render": True,
            "material_removed": True,
            "texture_blending": 228,
            "texture_blending_name": "REPLACE",
            "texture_wrapping_s": 240,
            "texture_wrapping_t": 240,
            "texture_wrapping_name": "CLAMP",
        },
    }


def export_m3g_reference(jar_path: Path, output_dir: Path) -> None:
    jar_path = Path(jar_path)
    output_dir = Path(output_dir)
    validate_canonical_jar(jar_path)
    if output_dir.exists():
        shutil.rmtree(output_dir)
    output_dir.mkdir(parents=True)

    with zipfile.ZipFile(jar_path) as jar:
        scene = parse_m3g(read_resource(jar, 45))

    _write_json(output_dir / "scene.json", _scene_summary(scene))
    _write_json(output_dir / "mesh_catalog.json", build_mesh_catalog(scene))

    textures_dir = output_dir / "textures"
    renders_dir = output_dir / "renders"
    gba_dir = output_dir / "gba"
    textures_dir.mkdir()
    renders_dir.mkdir()
    gba_dir.mkdir()

    texture_map: dict[int, TextureRGBA] = {}
    image_objects = [obj for obj in scene.objects[1:] if isinstance(obj, Image2D)]
    texture_manifest: list[dict[str, object]] = []
    for image in image_objects:
        width, height, rgba = decode_image_rgba(scene, image.object_index)
        texture_map[image.object_index] = TextureRGBA(width, height, rgba)
        relative_path = Path("textures") / f"image_{image.object_index:03d}.png"
        png = encode_png_rgba(width, height, rgba)
        (output_dir / relative_path).write_bytes(png)
        texture_manifest.append({
            "object_index": image.object_index,
            "user_id": image.user_id,
            "format": image.format,
            "width": width,
            "height": height,
            "rgba_sha256": sha256_bytes(rgba),
            "png_path": relative_path.as_posix(),
            "png_sha256": sha256_bytes(png),
        })

    camera = runtime_camera(_TARGET_WIDTH, _TARGET_HEIGHT)
    render_manifest: list[dict[str, object]] = []
    for user_id in GAME_MESH_USER_IDS:
        mesh = resolve_mesh(scene, user_id)
        frame = render_mesh_reference(
            mesh,
            camera=camera,
            textures=texture_map,
            draw_transform=_reference_draw_transform(mesh, camera),
        )
        render_relative = Path("renders") / f"mesh_{user_id:03d}.png"
        render_png = encode_png_rgba(frame.width, frame.height, frame.rgba)
        (output_dir / render_relative).write_bytes(render_png)

        raw_relative = Path("gba") / f"mesh_{user_id:03d}.bgr555le.bin"
        mask_relative = Path("gba") / f"mesh_{user_id:03d}.alpha1.bin"
        raw_bytes = _bgr555_frame(frame.rgba)
        mask_bytes = _alpha_mask(frame.rgba)
        (output_dir / raw_relative).write_bytes(raw_bytes)
        (output_dir / mask_relative).write_bytes(mask_bytes)

        indexed = _indexed_derivative(frame.rgba)
        indexed_path: str | None = None
        palette_path: str | None = None
        if indexed["indices"] is not None:
            bpp = indexed["index_bpp"]
            indexed_relative = Path("gba") / f"mesh_{user_id:03d}.{bpp}bpp.bin"
            palette_relative = Path("gba") / f"mesh_{user_id:03d}.{bpp}bpp.pal.bgr555le"
            (output_dir / indexed_relative).write_bytes(indexed["indices"])
            (output_dir / palette_relative).write_bytes(indexed["palette"])
            indexed_path = indexed_relative.as_posix()
            palette_path = palette_relative.as_posix()

        render_manifest.append({
            "user_id": user_id,
            "width": frame.width,
            "height": frame.height,
            "visible_bounds": None if frame.alpha_bounds is None else list(frame.alpha_bounds),
            "rgba_sha256": sha256_bytes(frame.rgba),
            "png_path": render_relative.as_posix(),
            "png_sha256": sha256_bytes(render_png),
            "bgr555_path": raw_relative.as_posix(),
            "bgr555_sha256": sha256_bytes(raw_bytes),
            "alpha_mask_path": mask_relative.as_posix(),
            "alpha_mask_sha256": sha256_bytes(mask_bytes),
            "unique_bgr555_colors": indexed["unique_bgr555_colors"],
            "palette_entries": indexed["palette_entries"],
            "transparent_pixels": indexed["transparent_pixels"],
            "partial_alpha_pixels": indexed["partial_alpha_pixels"],
            "four_bpp_lossless": indexed["four_bpp_lossless"],
            "eight_bpp_lossless": indexed["eight_bpp_lossless"],
            "indexed_path": indexed_path,
            "palette_path": palette_path,
        })

    manifest = {
        "source_jar_sha256": CANONICAL_SHA256,
        "source_resource_45_sha256": "41f755aeeeeb42a7cd1c9acaf642a4609e4d7993d910cf5c5fe6b217cba79ae1",
        "target": {"width": _TARGET_WIDTH, "height": _TARGET_HEIGHT},
        "render_parameters": {
            "base_fov": _BASE_FOV,
            "near": _NEAR,
            "far": _FAR,
            "pitch_degrees": _REFERENCE_PITCH_DEGREES,
            "yaw_degrees": _REFERENCE_YAW_DEGREES,
            "texture_filter": "nearest",
            "texture_blending": "REPLACE",
            "texture_wrapping": "CLAMP",
            "material_lighting": False,
        },
        "mesh_count": len(GAME_MESH_USER_IDS),
        "texture_count": len(texture_manifest),
        "textures": texture_manifest,
        "renders": render_manifest,
    }
    _write_json(output_dir / "manifest.json", manifest)
