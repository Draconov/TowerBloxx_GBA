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
    post_rotate,
    render_mesh_reference,
    runtime_camera,
)
from .resources import read_resource

BUTANO_VERSION = "21.7.1"

TUMBLE_MESH_IDS = (10, 11, 12, 13, 20, 21, 22, 23)
TUMBLE_STAGE_COUNT = 12

def tumble_pose_angles(stage: int, z_negative: bool, y_negative: bool) -> tuple[float, float]:
    if stage < 0:
        stage = 0
    elif stage > TUMBLE_STAGE_COUNT:
        stage = TUMBLE_STAGE_COUNT
    y_magnitude = float(stage * 5)
    z_magnitude = 45.0 * float(stage) / float(TUMBLE_STAGE_COUNT)
    return (-z_magnitude if z_negative else z_magnitude,
            -y_magnitude if y_negative else y_magnitude)


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



def _share_bpp8_asset_palette(graphics_dir: Path, asset_names: tuple[str, ...]) -> int:
    if not asset_names:
        raise ValueError("shared gameplay BPP8 palette group must not be empty")

    opaque: list[int] = []
    seen: set[int] = set()
    sources: list[tuple[str, Image.Image, tuple[int, ...]]] = []
    for asset_name in asset_names:
        bmp_path = graphics_dir / f"{asset_name}.bmp"
        metadata_path = graphics_dir / f"{asset_name}.json"
        metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
        old_bpp = 4 if metadata.get("bpp_mode") == "bpp_4" else 8
        image, palette = _read_indexed_asset(bmp_path, old_bpp)
        used = set(image.get_flattened_data())
        for index in sorted(used):
            if index == 0:
                continue
            color = palette[index]
            if color not in seen:
                seen.add(color)
                opaque.append(color)
        sources.append((asset_name, image.copy(), palette))
        image.close()

    if len(opaque) > 255:
        raise ValueError(f"shared gameplay BPP8 palette needs {len(opaque)} opaque colors")
    exact_entries = 1 + len(opaque)
    colors_count = ((exact_entries + 15) // 16) * 16
    canonical = (0, *opaque)
    color_to_index = {color: index + 1 for index, color in enumerate(opaque)}

    for asset_name, image, old_palette in sources:
        remapped = bytearray(image.width * image.height)
        for offset, old_index in enumerate(image.get_flattened_data()):
            if old_index:
                remapped[offset] = color_to_index[old_palette[old_index]]
        _write_indexed_bmp(
            graphics_dir / f"{asset_name}.bmp", image.width, image.height, bytes(remapped), canonical, 8
        )
        _write_json(
            graphics_dir / f"{asset_name}.json",
            {"bpp_mode": "bpp_8", "colors_count": colors_count, "type": "sprite"},
        )
    return colors_count


def _share_bpp4_asset_palette(graphics_dir: Path, asset_names: tuple[str, ...]) -> int:
    if not asset_names:
        raise ValueError("shared gameplay palette group must not be empty")

    opaque: list[int] = []
    seen: set[int] = set()
    sources: list[tuple[Path, Image.Image, tuple[int, ...]]] = []
    for asset_name in asset_names:
        bmp_path = graphics_dir / f"{asset_name}.bmp"
        metadata = json.loads((graphics_dir / f"{asset_name}.json").read_text(encoding="utf-8"))
        if metadata.get("bpp_mode") != "bpp_4":
            raise ValueError(f"{asset_name} is not a 4bpp gameplay sprite")
        image, palette = _read_indexed_asset(bmp_path, 4)
        used = set(image.get_flattened_data())
        for index in sorted(used):
            if index == 0:
                continue
            color = palette[index]
            if color not in seen:
                seen.add(color)
                opaque.append(color)
        sources.append((bmp_path, image.copy(), palette))
        image.close()

    if len(opaque) > 15:
        raise ValueError(f"shared gameplay BPP4 palette needs {len(opaque)} opaque colors")
    canonical = (0, *opaque)
    color_to_index = {color: index + 1 for index, color in enumerate(opaque)}
    for bmp_path, image, old_palette in sources:
        remapped = bytearray(image.width * image.height)
        for offset, old_index in enumerate(image.get_flattened_data()):
            if old_index:
                remapped[offset] = color_to_index[old_palette[old_index]]
        _write_indexed_bmp(bmp_path, image.width, image.height, bytes(remapped), canonical, 4)
    return len(canonical)

def _export_special_cable(graphics_dir: Path) -> None:
    """Regenerate House.e(Graphics)'s separate two-pixel special crane cable."""
    asset = "crane_special_cable_segment"
    # One 32x64 affine source line replaces the old chain of 8x16 segments.
    # 32x64 is a native GBA sprite size and its doubled affine canvas can cover
    # the full recovered cable length without introducing inter-segment gaps.
    width = 32
    height = 64
    indices = bytearray(width * height)
    for y in range(height):
        indices[y * width + 15] = 1
        indices[y * width + 16] = 1
    # Transparent magenta + opaque black.
    _write_indexed_bmp(
        graphics_dir / f"{asset}.bmp", width, height, bytes(indices), (0x7C1F, 0), 4
    )
    _write_json(graphics_dir / f"{asset}.json", {"bpp_mode": "bpp_4", "type": "sprite"})


def _asset_name(mesh_id: int, part_index: int) -> str:
    return f"tb_mesh_{mesh_id:03d}_p{part_index}"


def _crane_hook_asset_name(frame_index: int, part_index: int) -> str:
    return f"crane_hook_pose_{frame_index:02d}_p{part_index}"

def _tumble_asset_name(mesh_id: int, combo: int, stage: int, part_index: int) -> str:
    return f"tumble_m{mesh_id:03d}_c{combo}_s{stage:02d}_p{part_index}"


def _header(
    composites: list[SpriteComposite],
    crane_hook_frames: list[tuple[int, SpriteComposite]],
    tumble_poses: list[tuple[int, int, bool, bool, SpriteComposite]],
) -> str:
    names = [
        _asset_name(composite.mesh_id, part_index)
        for composite in composites
        for part_index, _part in enumerate(composite.parts)
    ]
    names.extend(
        _crane_hook_asset_name(frame_index, part_index)
        for frame_index, (_step, composite) in enumerate(crane_hook_frames)
        for part_index, _part in enumerate(composite.parts)
    )
    names.extend(
        _tumble_asset_name(mesh_id, (2 if z_negative else 0) + (1 if y_negative else 0), stage, part_index)
        for mesh_id, stage, z_negative, y_negative, composite in tumble_poses
        for part_index, _part in enumerate(composite.parts)
    )
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

    lines.extend([
        "struct CraneHookFrameAsset",
        "{",
        "    int16_t rotation_step;",
        "    const MeshPartAsset* parts;",
        "    int16_t part_count;",
        "};",
        "",
    ])
    for frame_index, (_step, composite) in enumerate(crane_hook_frames):
        lines.append(f"inline const MeshPartAsset crane_hook_pose_{frame_index:02d}_parts[] = {{")
        for part_index, part in enumerate(composite.parts):
            name = _crane_hook_asset_name(frame_index, part_index)
            x = part.source_x + part.width // 2 - composite.canvas_width // 2
            y = part.source_y + part.height // 2 - composite.canvas_height // 2
            lines.append(f"    {{ &bn::sprite_items::{name}, {x}, {y} }},")
        lines.extend(["};", ""])

    lines.append("inline const CraneHookFrameAsset crane_hook_frames[] = {")
    for frame_index, (step, composite) in enumerate(crane_hook_frames):
        lines.append(
            f"    {{ {step}, crane_hook_pose_{frame_index:02d}_parts, {len(composite.parts)} }},"
        )
    lines.extend([
        "};",
        f"inline constexpr int crane_hook_frame_count = {len(crane_hook_frames)};",
        "",
        "inline const CraneHookFrameAsset& crane_hook_frame_for_step(int step)",
        "{",
        "    if(step < -24)",
        "    {",
        "        step = -24;",
        "    }",
        "    else if(step > 24)",
        "    {",
        "        step = 24;",
        "    }",
        "    return crane_hook_frames[step + 24];",
        "}",
        "",
    ])

    lines.extend([
        "struct TumblePoseAsset",
        "{",
        "    int16_t mesh_id;",
        "    int8_t stage;",
        "    bool z_negative;",
        "    bool y_negative;",
        "    const MeshPartAsset* parts;",
        "    int16_t part_count;",
        "};",
        "",
    ])
    for pose_index, (mesh_id, stage, z_negative, y_negative, composite) in enumerate(tumble_poses):
        combo = (2 if z_negative else 0) + (1 if y_negative else 0)
        base = f"tumble_m{mesh_id:03d}_c{combo}_s{stage:02d}"
        lines.append(f"inline const MeshPartAsset {base}_parts[] = {{")
        for part_index, part in enumerate(composite.parts):
            name = _tumble_asset_name(mesh_id, combo, stage, part_index)
            x = part.source_x + part.width // 2 - composite.canvas_width // 2
            y = part.source_y + part.height // 2 - composite.canvas_height // 2
            lines.append(f"    {{ &bn::sprite_items::{name}, {x}, {y} }},")
        lines.extend(["};", ""])

    lines.append("inline const TumblePoseAsset tumble_poses[] = {")
    for mesh_id, stage, z_negative, y_negative, composite in tumble_poses:
        combo = (2 if z_negative else 0) + (1 if y_negative else 0)
        base = f"tumble_m{mesh_id:03d}_c{combo}_s{stage:02d}"
        lines.append(
            f"    {{ {mesh_id}, {stage}, {'true' if z_negative else 'false'}, "
            f"{'true' if y_negative else 'false'}, {base}_parts, {len(composite.parts)} }},"
        )
    lines.extend([
        "};",
        f"inline constexpr int tumble_pose_count = {len(tumble_poses)};",
        "",
        "inline int tumble_stage_for_y_angle(int y_angle_degrees)",
        "{",
        "    if(y_angle_degrees < 0)",
        "    {",
        "        y_angle_degrees = -y_angle_degrees;",
        "    }",
        "    int stage = (y_angle_degrees + 2) / 5;",
        "    if(stage > 12) { stage = 12; }",
        "    return stage;",
        "}",
        "",
        "inline bool tumble_pose_available(int mesh_id)",
        "{",
        "    return (mesh_id >= 10 && mesh_id <= 13) || (mesh_id >= 20 && mesh_id <= 23);",
        "}",
        "",
        "inline const TumblePoseAsset& tumble_pose_for(int mesh_id, int stage, bool z_negative, bool y_negative)",
        "{",
        "    int mesh_index = 0;",
        "    if(mesh_id >= 10 && mesh_id <= 13)",
        "    {",
        "        mesh_index = mesh_id - 10;",
        "    }",
        "    else if(mesh_id >= 20 && mesh_id <= 23)",
        "    {",
        "        mesh_index = 4 + mesh_id - 20;",
        "    }",
        "    if(stage < 1) { stage = 1; }",
        "    if(stage > 12) { stage = 12; }",
        "    const int combo = (z_negative ? 2 : 0) + (y_negative ? 1 : 0);",
        "    return tumble_poses[(mesh_index * 4 + combo) * 12 + (stage - 1)];",
        "}",
        "",
    ])

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

    # House.e(Graphics) rotates the complete mesh-8 hook/rope around one M3G
    # origin. Rotating its GBA sprite chunks independently shears the mesh, so
    # bake every reachable r=(crane_x >> 4) step with the original transform.
    crane_mesh = resolve_mesh(scene, 8)
    crane_textures: dict[int, TextureRGBA] = {}
    for submesh in crane_mesh.submeshes:
        image_index = submesh.image_index
        if image_index is not None and image_index not in crane_textures:
            width, height, rgba = decode_image_rgba(scene, image_index)
            crane_textures[image_index] = TextureRGBA(width, height, rgba)

    crane_hook_frames: list[tuple[int, SpriteComposite]] = []
    crane_hook_records: list[dict[str, object]] = []
    crane_hook_asset_names: list[str] = []
    for frame_index, rotation_step in enumerate(range(-24, 25)):
        draw_transform = post_rotate(
            identity_matrix(), rotation_step * (2.0 / 3.0), 0.0, 0.0, 1.0
        )
        frame = render_mesh_reference(
            crane_mesh, camera=camera, textures=crane_textures, draw_transform=draw_transform
        )
        render = Image.frombytes("RGBA", (camera.width, camera.height), frame.rgba)
        composite = slice_sprite(render, 800 + frame_index)
        if composite.bpp != 4:
            raise ValueError(
                f"crane hook pose {rotation_step} unexpectedly needs {composite.bpp}bpp"
            )
        crane_hook_frames.append((rotation_step, composite))
        part_records: list[dict[str, object]] = []
        for part_index, part in enumerate(composite.parts):
            name = _crane_hook_asset_name(frame_index, part_index)
            crane_hook_asset_names.append(name)
            _write_indexed_bmp(
                graphics_dir / f"{name}.bmp",
                part.width,
                part.height,
                part.indices,
                composite.palette_bgr555,
                composite.bpp,
            )
            _write_json(
                graphics_dir / f"{name}.json",
                {"bpp_mode": "bpp_4", "type": "sprite"},
            )
            part_records.append({
                "asset": name,
                "source_x": part.source_x,
                "source_y": part.source_y,
                "width": part.width,
                "height": part.height,
                "screen_x": part.source_x + part.width // 2 - composite.canvas_width // 2,
                "screen_y": part.source_y + part.height // 2 - composite.canvas_height // 2,
            })
        crane_hook_records.append({
            "rotation_step": rotation_step,
            "angle_degrees": rotation_step * (2.0 / 3.0),
            "bbox": list(composite.bbox),
            "bpp": composite.bpp,
            "palette_entries": len(composite.palette_bgr555),
            "parts": part_records,
        })

    crane_hook_palette_entries = _share_bpp4_asset_palette(
        graphics_dir, tuple(crane_hook_asset_names)
    )
    for record in crane_hook_records:
        record["palette_entries"] = crane_hook_palette_entries

    # Fix 15.2: bake the bad-placement secondary Y tumble through the same
    # M3G renderer as the source. Z and Y are applied around one shared origin
    # before projection. Five-degree Y stages keep the ROM cost bounded.
    tumble_poses: list[tuple[int, int, bool, bool, SpriteComposite]] = []
    tumble_records: list[dict[str, object]] = []
    tumble_assets_by_mesh: dict[int, list[str]] = {mesh_id: [] for mesh_id in TUMBLE_MESH_IDS}
    for mesh_id in TUMBLE_MESH_IDS:
        mesh = resolve_mesh(scene, mesh_id)
        textures: dict[int, TextureRGBA] = {}
        for submesh in mesh.submeshes:
            image_index = submesh.image_index
            if image_index is not None and image_index not in textures:
                width, height, rgba = decode_image_rgba(scene, image_index)
                textures[image_index] = TextureRGBA(width, height, rgba)
        for z_negative in (False, True):
            for y_negative in (False, True):
                combo = (2 if z_negative else 0) + (1 if y_negative else 0)
                for stage in range(1, 13):
                    z_angle, y_angle = tumble_pose_angles(stage, z_negative, y_negative)
                    draw_transform = post_rotate(
                        identity_matrix(), z_angle, 0.0, 0.0, 1.0
                    )
                    draw_transform = post_rotate(draw_transform, y_angle, 0.0, 1.0, 0.0)
                    frame = render_mesh_reference(
                        mesh, camera=camera, textures=textures, draw_transform=draw_transform
                    )
                    render = Image.frombytes("RGBA", (camera.width, camera.height), frame.rgba)
                    composite = slice_sprite(render, 10000 + len(tumble_poses))
                    tumble_poses.append((mesh_id, stage, z_negative, y_negative, composite))
                    part_records: list[dict[str, object]] = []
                    for part_index, part in enumerate(composite.parts):
                        name = _tumble_asset_name(mesh_id, combo, stage, part_index)
                        tumble_assets_by_mesh[mesh_id].append(name)
                        _write_indexed_bmp(
                            graphics_dir / f"{name}.bmp", part.width, part.height, part.indices,
                            composite.palette_bgr555, composite.bpp
                        )
                        _write_json(
                            graphics_dir / f"{name}.json",
                            {"bpp_mode": f"bpp_{composite.bpp}", "type": "sprite"},
                        )
                        part_records.append({
                            "asset": name,
                            "source_x": part.source_x,
                            "source_y": part.source_y,
                            "width": part.width,
                            "height": part.height,
                            "screen_x": part.source_x + part.width // 2 - composite.canvas_width // 2,
                            "screen_y": part.source_y + part.height // 2 - composite.canvas_height // 2,
                        })
                    tumble_records.append({
                        "mesh_id": mesh_id, "stage": stage,
                        "z_negative": z_negative, "y_negative": y_negative,
                        "z_angle_degrees": z_angle, "y_angle_degrees": y_angle,
                        "bbox": list(composite.bbox), "bpp": composite.bpp,
                        "palette_entries": len(composite.palette_bgr555), "parts": part_records,
                    })

    # Floor, initial/base floor, normal roof and trophy roof from one tower
    # family can be live simultaneously. They must share the same partial
    # BPP8 OBJ palette; otherwise the GBA has only one incompatible BPP8
    # palette space. Fix 14.6 adds the recovered 20..23 base meshes to the
    # family because the first landed floor persists under later floors.
    for family in ((10, 20, 30, 40), (11, 21, 31, 41), (12, 22, 32, 42), (13, 23, 33, 43)):
        _share_mesh_family_palette(graphics_dir, mesh_records, family)
        family_asset_names: list[str] = []
        for family_mesh_id in family:
            record = next(record for record in mesh_records if record["mesh_id"] == family_mesh_id)
            family_asset_names.extend(str(part["asset"]) for part in record["parts"])
        family_asset_names.extend(tumble_assets_by_mesh[family[0]])
        family_asset_names.extend(tumble_assets_by_mesh[family[1]])
        colors_count = _share_bpp8_asset_palette(graphics_dir, tuple(family_asset_names))
        for family_mesh_id in family:
            record = next(record for record in mesh_records if record["mesh_id"] == family_mesh_id)
            record["bpp"] = 8
            record["palette_entries"] = colors_count
        for record in tumble_records:
            if record["mesh_id"] in (family[0], family[1]):
                record["bpp"] = 8
                record["palette_entries"] = colors_count

    # Fix 13's special cable is a generated gameplay asset too. Recreate it
    # after the clean graphics-directory reset.
    _export_special_cable(graphics_dir)
    mesh7_record = next(record for record in mesh_records if record["mesh_id"] == 7)
    mesh8_record = next(record for record in mesh_records if record["mesh_id"] == 8)
    mesh9_record = next(record for record in mesh_records if record["mesh_id"] == 9)
    mesh7_names = tuple(str(part["asset"]) for part in mesh7_record["parts"])
    mesh8_names = tuple(str(part["asset"]) for part in mesh8_record["parts"])
    mesh9_names = tuple(str(part["asset"]) for part in mesh9_record["parts"])

    # Fix 15.4: the special rig, normal pre-rendered hook and platform are
    # mutually compatible exact-color BPP4 assets (nine opaque colors total).
    # Put them on one hardware bank so the 128-color tower BPP8 palette still
    # leaves enough banks for workers and the complete Quick Game HUD.
    crane_hook_palette_entries = _share_bpp4_asset_palette(
        graphics_dir,
        (*mesh7_names, *mesh8_names, *mesh9_names, *crane_hook_asset_names,
         "crane_special_cable_segment"),
    )
    mesh7_record["palette_entries"] = crane_hook_palette_entries
    mesh8_record["palette_entries"] = crane_hook_palette_entries
    mesh9_record["palette_entries"] = crane_hook_palette_entries
    for record in crane_hook_records:
        record["palette_entries"] = crane_hook_palette_entries

    header_path = include_dir / "tower_mesh_assets.h"
    header_path.write_text(_header(composites, crane_hook_frames, tumble_poses), encoding="utf-8", newline="\n")

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
        "crane_hook_frames": crane_hook_records,
        "crane_hook_palette_entries": crane_hook_palette_entries,
        "tumble_pose_stage_degrees": 5,
        "tumble_pose_count": len(tumble_poses),
        "tumble_poses": tumble_records,
        "files": files,
        "tree_hash": tree_digest.hexdigest(),
    }
    _write_json(reference_dir / "generated_assets_manifest.json", manifest)
    return manifest
