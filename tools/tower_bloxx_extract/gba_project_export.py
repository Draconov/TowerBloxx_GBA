from __future__ import annotations

import hashlib
import json
from pathlib import Path
import shutil
import struct
import zipfile

from PIL import Image

from .gba_assets import SpriteComposite, slice_sprite
from .m3g import parse_m3g
from .m3g_geometry import GAME_MESH_USER_IDS, decode_image_rgba, resolve_mesh
from .m3g_render import (
    TextureRGBA,
    class_n_camera_transform,
    house_gameplay_camera_distance,
    identity_matrix,
    render_mesh_reference,
    runtime_camera,
)
from .resources import read_resource

BUTANO_VERSION = "21.7.1"


def _sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _write_json(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, sort_keys=True, indent=2) + "\n", encoding="utf-8", newline="\n")


def _bgr555_rgb(value: int) -> tuple[int, int, int]:
    return (
        (value & 0x1F) << 3,
        ((value >> 5) & 0x1F) << 3,
        ((value >> 10) & 0x1F) << 3,
    )


def _write_indexed_bmp(path: Path, width: int, height: int, indices: bytes, palette: tuple[int, ...], bpp: int) -> None:
    if bpp not in (4, 8):
        raise ValueError("indexed BMP must be 4bpp or 8bpp")
    if len(indices) != width * height:
        raise ValueError("index byte count does not match dimensions")
    palette_size = 16 if bpp == 4 else 256
    if len(palette) > palette_size:
        raise ValueError("palette is too large for BMP depth")
    if indices and max(indices) >= len(palette):
        raise ValueError("pixel references missing palette entry")

    if bpp == 8:
        raw_row_bytes = width
    else:
        raw_row_bytes = (width + 1) // 2
    stored_row_bytes = (raw_row_bytes + 3) & ~3
    pixel_bytes = bytearray(stored_row_bytes * height)

    for output_row, source_y in enumerate(range(height - 1, -1, -1)):
        source = indices[source_y * width : (source_y + 1) * width]
        row_offset = output_row * stored_row_bytes
        if bpp == 8:
            pixel_bytes[row_offset : row_offset + width] = source
        else:
            for x in range(0, width, 2):
                high = source[x] & 0x0F
                low = source[x + 1] & 0x0F if x + 1 < width else 0
                pixel_bytes[row_offset + x // 2] = (high << 4) | low

    color_table = bytearray()
    for index in range(palette_size):
        value = palette[index] if index < len(palette) else 0
        red, green, blue = _bgr555_rgb(value)
        color_table.extend((blue, green, red, 0))

    data_offset = 14 + 40 + len(color_table)
    file_size = data_offset + len(pixel_bytes)
    file_header = struct.pack("<2sIHHI", b"BM", file_size, 0, 0, data_offset)
    info_header = struct.pack(
        "<IiiHHIIiiII",
        40, width, height, 1, bpp, 0, len(pixel_bytes), 2835, 2835, palette_size, palette_size,
    )
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(file_header + info_header + bytes(color_table) + bytes(pixel_bytes))



def _read_indexed_asset(path: Path, bpp: int) -> tuple[Image.Image, tuple[int, ...]]:
    image = Image.open(path)
    if image.mode != "P":
        image.close()
        raise ValueError(f"{path.name} is not indexed")
    raw_palette = image.getpalette()
    if raw_palette is None:
        image.close()
        raise ValueError(f"{path.name} has no palette")
    entries = 16 if bpp == 4 else 256
    palette: list[int] = []
    for index in range(entries):
        base = index * 3
        red, green, blue = raw_palette[base : base + 3]
        palette.append((red >> 3) | ((green >> 3) << 5) | ((blue >> 3) << 10))
    return image, tuple(palette)


def _share_mesh_family_palette(
    graphics_dir: Path,
    mesh_records: list[dict[str, object]],
    mesh_ids: tuple[int, ...],
) -> int:
    records_by_id = {int(record["mesh_id"]): record for record in mesh_records}
    records = [records_by_id[mesh_id] for mesh_id in mesh_ids]
    opaque: list[int] = []
    seen: set[int] = set()
    sources: list[tuple[dict[str, object], str, Image.Image, tuple[int, ...]]] = []

    for record in records:
        old_bpp = int(record["bpp"])
        for part in record["parts"]:  # type: ignore[index]
            asset = str(part["asset"])
            image, palette = _read_indexed_asset(graphics_dir / f"{asset}.bmp", old_bpp)
            used = set(image.get_flattened_data())
            for index in sorted(used):
                if index == 0:
                    continue
                color = palette[index]
                if color not in seen:
                    seen.add(color)
                    opaque.append(color)
            sources.append((record, asset, image.copy(), palette))
            image.close()

    if len(opaque) > 255:
        raise ValueError(f"tower family {mesh_ids!r} needs {len(opaque)} opaque colors")
    exact_entries = 1 + len(opaque)
    colors_count = ((exact_entries + 15) // 16) * 16
    canonical = (0, *opaque)
    color_to_index = {color: index + 1 for index, color in enumerate(opaque)}

    for _record, asset, image, old_palette in sources:
        remapped = bytearray(image.width * image.height)
        for offset, old_index in enumerate(image.get_flattened_data()):
            if old_index:
                remapped[offset] = color_to_index[old_palette[old_index]]
        _write_indexed_bmp(
            graphics_dir / f"{asset}.bmp",
            image.width,
            image.height,
            bytes(remapped),
            canonical,
            8,
        )
        _write_json(
            graphics_dir / f"{asset}.json",
            {"bpp_mode": "bpp_8", "colors_count": colors_count, "type": "sprite"},
        )

    for record in records:
        record["bpp"] = 8
        record["palette_entries"] = colors_count
    return colors_count


def _export_special_cable(graphics_dir: Path) -> None:
    """Regenerate House.e(Graphics)'s separate two-pixel special-roof cable."""
    asset = "crane_special_cable_segment"
    width = 8
    height = 16
    indices = bytearray(width * height)
    for y in range(height):
        indices[y * width + 3] = 1
        indices[y * width + 4] = 1
    # Transparent magenta + opaque black, matching the hand-recovered Fix 13 asset.
    _write_indexed_bmp(
        graphics_dir / f"{asset}.bmp", width, height, bytes(indices), (0x7C1F, 0), 4
    )
    _write_json(graphics_dir / f"{asset}.json", {"bpp_mode": "bpp_4", "type": "sprite"})


def _asset_name(mesh_id: int, part_index: int) -> str:
    return f"tb_mesh_{mesh_id:03d}_p{part_index}"


def _header(composites: list[SpriteComposite]) -> str:
    names = [
        _asset_name(composite.mesh_id, part_index)
        for composite in composites
        for part_index, _part in enumerate(composite.parts)
    ]
    lines = [
        "#ifndef TB_GENERATED_TOWER_MESH_ASSETS_H",
        "#define TB_GENERATED_TOWER_MESH_ASSETS_H",
        "",
        "#include <cstdint>",
    ]
    lines.extend(f'#include "bn_sprite_items_{name}.h"' for name in names)
    lines.extend([
        "",
        "namespace tb::generated",
        "{",
        "struct MeshPartAsset",
        "{",
        "    const bn::sprite_item* item;",
        "    int16_t x;",
        "    int16_t y;",
        "};",
        "",
        "struct MeshAsset",
        "{",
        "    int16_t mesh_id;",
        "    const MeshPartAsset* parts;",
        "    int16_t part_count;",
        "};",
        "",
    ])
    for composite in composites:
        lines.append(f"inline const MeshPartAsset mesh_{composite.mesh_id:03d}_parts[] = {{")
        for part_index, part in enumerate(composite.parts):
            name = _asset_name(composite.mesh_id, part_index)
            x = part.source_x + part.width // 2 - composite.canvas_width // 2
            y = part.source_y + part.height // 2 - composite.canvas_height // 2
            lines.append(f"    {{ &bn::sprite_items::{name}, {x}, {y} }},")
        lines.extend(["};", ""])
    lines.append("inline const MeshAsset meshes[] = {")
    for composite in composites:
        lines.append(
            f"    {{ {composite.mesh_id}, mesh_{composite.mesh_id:03d}_parts, "
            f"{len(composite.parts)} }},"
        )
    lines.extend([
        "};",
        f"inline constexpr int mesh_count = {len(composites)};",
        "}",
        "",
        "#endif",
        "",
    ])
    return "\n".join(lines)


def export_gba_project_assets(jar_path: Path, project_dir: Path) -> dict[str, object]:
    jar_path = Path(jar_path)
    project_dir = Path(project_dir)
    graphics_dir = project_dir / "gba" / "graphics" / "gameplay"
    include_dir = project_dir / "gba" / "include" / "generated"
    reference_dir = project_dir / "gba" / "reference"
    if graphics_dir.exists():
        shutil.rmtree(graphics_dir)
    graphics_dir.mkdir(parents=True, exist_ok=True)
    # Generated headers share this directory with the font/localization/UI
    # exporter. Never remove siblings that belong to another asset pipeline.
    include_dir.mkdir(parents=True, exist_ok=True)
    reference_dir.mkdir(parents=True, exist_ok=True)

    composites: list[SpriteComposite] = []
    mesh_records: list[dict[str, object]] = []
    camera_z = house_gameplay_camera_distance(240, 160)
    camera = runtime_camera(
        240,
        160,
        base_fov=55.0,
        transform=class_n_camera_transform(
            position=(0.0, 0.0, float(camera_z)),
            direction=(0.0, 0.0, -1.0),
            up=(0.0, 1.0, 0.0),
        ),
    )

    with zipfile.ZipFile(jar_path) as jar:
        scene = parse_m3g(read_resource(jar, 45))

    for mesh_id in GAME_MESH_USER_IDS:
        mesh = resolve_mesh(scene, mesh_id)
        textures: dict[int, TextureRGBA] = {}
        for submesh in mesh.submeshes:
            image_index = submesh.image_index
            if image_index is not None and image_index not in textures:
                width, height, rgba = decode_image_rgba(scene, image_index)
                textures[image_index] = TextureRGBA(width, height, rgba)

        frame = render_mesh_reference(
            mesh, camera=camera, textures=textures, draw_transform=identity_matrix()
        )
        render = Image.frombytes("RGBA", (camera.width, camera.height), frame.rgba)
        composite = slice_sprite(render, mesh_id)
        composites.append(composite)
        part_records: list[dict[str, object]] = []
        for part_index, part in enumerate(composite.parts):
            name = _asset_name(mesh_id, part_index)
            bmp_path = graphics_dir / f"{name}.bmp"
            json_path = graphics_dir / f"{name}.json"
            _write_indexed_bmp(
                bmp_path, part.width, part.height, part.indices, composite.palette_bgr555, composite.bpp
            )
            _write_json(json_path, {"bpp_mode": f"bpp_{composite.bpp}", "type": "sprite"})
            part_records.append({
                "asset": name,
                "source_x": part.source_x,
                "source_y": part.source_y,
                "width": part.width,
                "height": part.height,
                "screen_x": part.source_x + part.width // 2 - composite.canvas_width // 2,
                "screen_y": part.source_y + part.height // 2 - composite.canvas_height // 2,
            })
        mesh_records.append({
            "mesh_id": mesh_id,
            "bbox": list(composite.bbox),
            "bpp": composite.bpp,
            "palette_entries": len(composite.palette_bgr555),
            "parts": part_records,
        })

    # Fix 14.3: floor, normal roof and trophy roof from one tower family can
    # be live simultaneously.  They must share the same partial BPP8 OBJ
    # palette; otherwise the GBA has only one incompatible BPP8 palette space.
    for family in ((10, 30, 40), (11, 31, 41), (12, 32, 42), (13, 33, 43)):
        _share_mesh_family_palette(graphics_dir, mesh_records, family)

    # Fix 13's special roof cable is a generated gameplay asset too.  Recreate
    # it after the clean graphics-directory reset so clean builds retain it.
    _export_special_cable(graphics_dir)

    header_path = include_dir / "tower_mesh_assets.h"
    header_path.write_text(_header(composites), encoding="utf-8", newline="\n")

    tracked_files = sorted([
        *graphics_dir.glob("*.bmp"),
        *graphics_dir.glob("*.json"),
        header_path,
    ])
    files = [
        {
            "path": path.relative_to(project_dir).as_posix(),
            "sha256": _sha256(path),
            "size": path.stat().st_size,
        }
        for path in tracked_files
    ]
    tree_digest = hashlib.sha256()
    for record in files:
        tree_digest.update(record["path"].encode())
        tree_digest.update(b"\0")
        tree_digest.update(record["sha256"].encode())
        tree_digest.update(b"\n")

    manifest: dict[str, object] = {
        "butano_version": BUTANO_VERSION,
        "mesh_count": len(composites),
        "render_pose": "house_gameplay_240x160",
        "camera_z": camera_z,
        "base_fov": 55.0,
        "mesh_ids": list(GAME_MESH_USER_IDS),
        "meshes": mesh_records,
        "files": files,
        "tree_hash": tree_digest.hexdigest(),
    }
    _write_json(reference_dir / "generated_assets_manifest.json", manifest)
    return manifest
