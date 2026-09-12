from __future__ import annotations

import hashlib
import json
from pathlib import Path
import shutil
import struct
import tempfile

from PIL import Image

from .gba_assets import SpriteComposite, slice_sprite
from .m3g_export import export_m3g_reference
from .m3g_geometry import GAME_MESH_USER_IDS

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
    graphics_dir = project_dir / "gba" / "graphics" / "generated"
    include_dir = project_dir / "gba" / "include" / "generated"
    reference_dir = project_dir / "gba" / "reference"
    for directory in (graphics_dir, include_dir):
        if directory.exists():
            shutil.rmtree(directory)
        directory.mkdir(parents=True, exist_ok=True)
    reference_dir.mkdir(parents=True, exist_ok=True)

    composites: list[SpriteComposite] = []
    mesh_records: list[dict[str, object]] = []
    with tempfile.TemporaryDirectory(prefix="tower-bloxx-m3g-") as tmp:
        m3g_dir = Path(tmp) / "m3g"
        export_m3g_reference(jar_path, m3g_dir)
        for mesh_id in GAME_MESH_USER_IDS:
            render = Image.open(m3g_dir / "renders" / f"mesh_{mesh_id:03d}.png").convert("RGBA")
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
        "mesh_ids": list(GAME_MESH_USER_IDS),
        "meshes": mesh_records,
        "files": files,
        "tree_hash": tree_digest.hexdigest(),
    }
    _write_json(reference_dir / "generated_assets_manifest.json", manifest)
    return manifest
