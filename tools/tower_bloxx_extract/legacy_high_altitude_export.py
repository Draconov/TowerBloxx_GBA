from __future__ import annotations

from io import BytesIO
import hashlib
import json
from pathlib import Path
import struct
import zipfile

from PIL import Image

from .gba_assets import choose_sprite_shape
from .gba_project_export import _write_indexed_bmp

# House.class from the 208x208 Nokia v1.3.37 build. Index 0 is the source's
# "no event" sentinel; playable sky-event types are 1..28.
LEGACY_EVENT_RESOURCE_IDS = (
    -1,
    50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1,
    62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76,
)
LEGACY_EVENT_MIN_BAND = (
    0,
    0, 0, 2, 3, 2, 3, 3, 4, 5, 6, 6, 6, 7, 8, 8, 9, 9, 10, 10, 11, 12, 12, 13, 14, 15, 17, 18, 21,
)
LEGACY_EVENT_MAX_BAND = (
    0,
    1, 1, 3, 4, 4, 4, 5, 5, 6, 7, 7, 7, 999, 9, 10, 999, 999, 11, 999, 12, 13, 14, 999, 15, 16, 18, 19, 999,
)
LEGACY_EVENT_SPAWN_CHANCE = (
    0,
    30, 30, 20, 20, 40, 60, 60, 30, 30, 50, 50, 10, 50, 100, 20, 40, 50, 100, 30, 100, 100, 30, 10, 100, 100, 100, 100, 100,
)
LEGACY_EVENT_X_SPEED = (
    0,
    2, 1, 3, 2, 2, -2, 3, -4, 5, 2, 2, 6, 0, 0, -2, 0, 0, 0, 0, 0, 0, 3, -3, 0, 0, 0, 0, -3,
)
LEGACY_EVENT_INSTANCE_LIMIT = (
    0,
    3, 2, 1, 1, 8, 2, 8, 1, 1, 4, 4, -1, 8, -1, 1, 5, 4, -1, 5, 2, -1, 1, 1, -1, -1, -1, -1, -1,
)
LEGACY_EVENT_NAMES = (
    'none',
    'cloud_line_a', 'cloud_line_b', 'plane', 'balloon', 'cloud_bank_a', 'birds',
    'cloud_bank_b', 'contrail_a', 'contrail_b', 'cloud_strip', 'cloud_bank_c', 'flyers',
    'star_dot', 'moon', 'satellite', 'magenta_dot', 'magenta_star', 'mars', 'cyan_star',
    'asteroid_belt', 'jupiter', 'rocket', 'ufo', 'saturn', 'uranus', 'neptune', 'icy_moon', 'whale',
)

# House.e(Graphics): types 6 and 12 use two horizontal half-width frames;
# type 28 uses two vertical half-height frames. All others are static.
LEGACY_EVENT_FRAME_AXIS = {6: 'x', 12: 'x', 28: 'y'}


def _parse_r0(raw: bytes) -> tuple[int, ...]:
    if len(raw) < 4:
        raise ValueError('r0 is too short')
    table_bytes = struct.unpack('>i', raw[:4])[0]
    if table_bytes <= 4 or table_bytes % 4:
        raise ValueError(f'invalid r0 table byte count: {table_bytes}')
    if table_bytes > len(raw):
        raise ValueError('r0 table exceeds file')
    return struct.unpack(f'>{table_bytes // 4}i', raw[:table_bytes])


def read_legacy_resource(jar: zipfile.ZipFile, resource_id: int) -> bytes:
    raw = jar.read('r0')
    table = _parse_r0(raw)
    if not 0 <= resource_id < len(table) - 1:
        raise IndexError(resource_id)
    start = table[resource_id]
    if start < 0:
        return jar.read(str(resource_id))
    end = len(raw)
    for value in table[resource_id + 1:]:
        if value >= 0 and value > start:
            end = value
            break
    return raw[start:end]


def _load_png(jar: zipfile.ZipFile, resource_id: int) -> Image.Image:
    data = read_legacy_resource(jar, resource_id)
    if not data.startswith(b'\x89PNG\r\n\x1a\n'):
        raise ValueError(f'legacy resource {resource_id} is not PNG')
    return Image.open(BytesIO(data)).convert('RGBA')


def _frames_for_event(event_type: int, image: Image.Image) -> tuple[Image.Image, ...]:
    axis = LEGACY_EVENT_FRAME_AXIS.get(event_type)
    if axis == 'x':
        half = image.width // 2
        return (image.crop((0, 0, half, image.height)), image.crop((half, 0, image.width, image.height)))
    if axis == 'y':
        half = image.height // 2
        return (image.crop((0, 0, image.width, half)), image.crop((0, half, image.width, image.height)))
    return (image,)


def _shared_palette(images: list[Image.Image]) -> tuple[tuple[int, int, int], ...]:
    pixels: list[tuple[int, int, int]] = []
    for image in images:
        for red, green, blue, alpha in image.get_flattened_data():
            if alpha:
                pixels.append((red, green, blue))
    sample = Image.new('RGB', (len(pixels), 1))
    sample.putdata(pixels)
    quantized = sample.quantize(colors=15, method=Image.Quantize.MEDIANCUT)
    raw = quantized.getpalette() or []
    used = sorted(set(quantized.get_flattened_data()))
    colors = [tuple(raw[index * 3:index * 3 + 3]) for index in used]
    # Quantize once more to GBA RGB555 so every output BMP truly shares bytes.
    unique: list[tuple[int, int, int]] = []
    for red, green, blue in colors:
        color = ((red >> 3) << 3, (green >> 3) << 3, (blue >> 3) << 3)
        if color not in unique:
            unique.append(color)
    return tuple(unique[:15])


def _nearest_palette_index(rgb: tuple[int, int, int], palette: tuple[tuple[int, int, int], ...]) -> int:
    red, green, blue = rgb
    best = 0
    best_distance = 1 << 60
    for index, (pr, pg, pb) in enumerate(palette):
        distance = (red - pr) ** 2 + (green - pg) ** 2 + (blue - pb) ** 2
        if distance < best_distance:
            best_distance = distance
            best = index
    return best + 1


def _bgr555(rgb: tuple[int, int, int]) -> int:
    red, green, blue = rgb
    return (red >> 3) | ((green >> 3) << 5) | ((blue >> 3) << 10)


def _write_composite(
    image: Image.Image,
    name: str,
    graphics_dir: Path,
    palette: tuple[tuple[int, int, int], ...],
) -> dict[str, object]:
    rgba = image.convert('RGBA')
    bbox = rgba.getchannel('A').getbbox()
    if bbox is None:
        raise ValueError(f'{name} has no visible pixels')
    left, top, right, bottom = bbox
    gba_palette = (0, *(_bgr555(color) for color in palette))
    parts: list[dict[str, object]] = []
    cache: dict[tuple[int, int, int], int] = {}
    part_index = 0
    for source_y in range(top, bottom, 64):
        needed_height = min(64, bottom - source_y)
        for source_x in range(left, right, 64):
            needed_width = min(64, right - source_x)
            width, height = choose_sprite_shape(needed_width, needed_height)
            indices = bytearray(width * height)
            for local_y in range(height):
                canvas_y = source_y + local_y
                if canvas_y >= rgba.height:
                    continue
                for local_x in range(width):
                    canvas_x = source_x + local_x
                    if canvas_x >= rgba.width:
                        continue
                    red, green, blue, alpha = rgba.getpixel((canvas_x, canvas_y))
                    if not alpha:
                        continue
                    key = (red, green, blue)
                    palette_index = cache.get(key)
                    if palette_index is None:
                        palette_index = _nearest_palette_index(key, palette)
                        cache[key] = palette_index
                    indices[local_y * width + local_x] = palette_index
            asset = f'{name}_p{part_index}'
            _write_indexed_bmp(
                graphics_dir / f'{asset}.bmp', width, height, bytes(indices), gba_palette, 4
            )
            (graphics_dir / f'{asset}.json').write_text(
                json.dumps({'bpp_mode': 'bpp_4', 'type': 'sprite'}, sort_keys=True) + '\n',
                encoding='utf-8',
            )
            parts.append({
                'asset': asset,
                'source_x': source_x,
                'source_y': source_y,
                'width': width,
                'height': height,
                'screen_x': source_x + width // 2 - rgba.width // 2,
                'screen_y': source_y + height // 2 - rgba.height // 2,
            })
            part_index += 1
    return {'name': name, 'width': rgba.width, 'height': rgba.height, 'parts': parts}


def _cpp_array(values: tuple[int, ...]) -> str:
    return ', '.join(str(value) for value in values)


def _write_header(project_dir: Path, records: dict[str, dict[str, object]], event_frames: dict[int, list[str]]) -> Path:
    path = project_dir / 'gba/include/generated/legacy_high_altitude_assets.h'
    path.parent.mkdir(parents=True, exist_ok=True)
    includes: list[str] = []
    bodies: list[str] = []
    for name, record in records.items():
        part_lines: list[str] = []
        for part in record['parts']:  # type: ignore[index]
            asset = str(part['asset'])
            includes.append(f'#include "bn_sprite_items_{asset}.h"')
            part_lines.append(
                f'    {{ &bn::sprite_items::{asset}, {int(part["screen_x"])}, {int(part["screen_y"])} }},'
            )
        bodies.append(
            f'inline const UiSpritePartAsset {name}_parts[] = {{\n' + '\n'.join(part_lines) +
            f'\n}};\ninline const UiCompositeAsset {name} = {{ {name}_parts, {len(part_lines)} }};\n'
        )

    event_asset_lines = ['    { nullptr, 0, 0, 0 },']
    for event_type in range(1, 29):
        frame_names = event_frames[event_type]
        arr_name = f'legacy_sky_type_{event_type}_frames'
        bodies.append(
            f'inline const UiCompositeAsset* const {arr_name}[] = {{ ' +
            ', '.join(f'&{name}' for name in frame_names) + ' };\n'
        )
        first = records[frame_names[0]]
        event_asset_lines.append(
            f'    {{ {arr_name}, {len(frame_names)}, {int(first["width"])}, {int(first["height"])} }},'
        )

    combo_names = [f'legacy_combo_star_f{index}' for index in range(4)]
    burst_names = [f'legacy_block_sparkle_f{index}' for index in range(3)]
    bodies.append(
        'inline const UiCompositeAsset* const legacy_combo_star_frames[] = { ' +
        ', '.join(f'&{name}' for name in combo_names) + ' };\n'
    )
    bodies.append(
        'inline const UiCompositeAsset* const legacy_block_sparkle_frames[] = { ' +
        ', '.join(f'&{name}' for name in burst_names) + ' };\n'
    )

    header = '''#ifndef TB_GENERATED_LEGACY_HIGH_ALTITUDE_ASSETS_H\n#define TB_GENERATED_LEGACY_HIGH_ALTITUDE_ASSETS_H\n\n#include "generated/tower_ui_assets.h"\n'''
    header += '\n'.join(sorted(set(includes))) + '\n\nnamespace tb::generated\n{\n'
    header += '\n'.join(bodies)
    header += '''\nstruct LegacySkyEventAsset\n{\n    const UiCompositeAsset* const* frames;\n    int16_t frame_count;\n    int16_t width;\n    int16_t height;\n};\n\n'''
    header += f'inline constexpr int legacy_event_resource_ids[] = {{ {_cpp_array(LEGACY_EVENT_RESOURCE_IDS)} }};\n'
    header += f'inline constexpr int legacy_event_min_band[] = {{ {_cpp_array(LEGACY_EVENT_MIN_BAND)} }};\n'
    header += f'inline constexpr int legacy_event_max_band[] = {{ {_cpp_array(LEGACY_EVENT_MAX_BAND)} }};\n'
    header += f'inline constexpr int legacy_event_spawn_chance[] = {{ {_cpp_array(LEGACY_EVENT_SPAWN_CHANCE)} }};\n'
    header += f'inline constexpr int legacy_event_x_speed[] = {{ {_cpp_array(LEGACY_EVENT_X_SPEED)} }};\n'
    header += f'inline constexpr int legacy_event_instance_limits[] = {{ {_cpp_array(LEGACY_EVENT_INSTANCE_LIMIT)} }};\n'
    extents = [0]
    for event_type in range(1, 29):
        first = records[event_frames[event_type][0]]
        extent = 1 if event_type == 13 else 8 * max(int(first['width']), int(first['height']))
        extents.append(extent)
    header += f'inline constexpr int legacy_event_extent_eighths[] = {{ {", ".join(str(value) for value in extents)} }};\n'
    header += 'inline const LegacySkyEventAsset legacy_sky_event_assets[] = {\n' + '\n'.join(event_asset_lines) + '\n};\n'
    header += '}\n\n#endif\n'
    path.write_text(header, encoding='utf-8')
    return path


def export_legacy_high_altitude_assets(legacy_jar_path: Path, project_dir: Path) -> dict[str, object]:
    legacy_jar_path = Path(legacy_jar_path)
    project_dir = Path(project_dir)
    graphics_dir = project_dir / 'gba/graphics/ui'
    graphics_dir.mkdir(parents=True, exist_ok=True)

    # Remove superseded placeholder/misnamed assets from earlier parity passes.
    for pattern in ('construction_event_*', 'quick_combo_star_p0.*', 'legacy_landing_burst_*'):
        for obsolete in graphics_dir.glob(pattern):
            if obsolete.is_file():
                obsolete.unlink()

    records: dict[str, dict[str, object]] = {}
    event_frames: dict[int, list[str]] = {}
    source_images: list[Image.Image] = []
    staged: list[tuple[str, Image.Image]] = []

    with zipfile.ZipFile(legacy_jar_path) as jar:
        combo_strip = _load_png(jar, 46)
        for frame in range(4):
            image = combo_strip.crop((frame * 22, 0, (frame + 1) * 22, 22))
            name = f'legacy_combo_star_f{frame}'
            staged.append((name, image)); source_images.append(image)

        burst_strip = _load_png(jar, 47)
        for frame in range(3):
            image = burst_strip.crop((frame * 44, 0, (frame + 1) * 44, 44))
            name = f'legacy_block_sparkle_f{frame}'
            staged.append((name, image)); source_images.append(image)

        for event_type in range(1, 29):
            name = LEGACY_EVENT_NAMES[event_type]
            resource_id = LEGACY_EVENT_RESOURCE_IDS[event_type]
            if event_type == 13:
                image = Image.new('RGBA', (8, 8), (0, 0, 0, 0))
                image.putpixel((4, 4), (255, 255, 255, 255))
                frames = (image,)
            else:
                frames = _frames_for_event(event_type, _load_png(jar, resource_id))
            frame_names: list[str] = []
            for frame_index, image in enumerate(frames):
                asset_name = f'legacy_sky_{name}_f{frame_index}'
                frame_names.append(asset_name)
                staged.append((asset_name, image)); source_images.append(image)
            event_frames[event_type] = frame_names

    palette = _shared_palette(source_images)
    for name, image in staged:
        records[name] = _write_composite(image, name, graphics_dir, palette)

    header = _write_header(project_dir, records, event_frames)
    manifest = {
        'source_jar_sha256': hashlib.sha256(legacy_jar_path.read_bytes()).hexdigest(),
        'source_version': 'Nokia 6230i 208x208 v1.3.37',
        'source_resources': {'combo_star': 46, 'block_sparkle': 47, 'sky_events': list(range(50, 77))},
        'event_resource_ids': list(LEGACY_EVENT_RESOURCE_IDS),
        'event_min_band': list(LEGACY_EVENT_MIN_BAND),
        'event_max_band': list(LEGACY_EVENT_MAX_BAND),
        'event_spawn_chance': list(LEGACY_EVENT_SPAWN_CHANCE),
        'event_x_speed': list(LEGACY_EVENT_X_SPEED),
        'event_instance_limits': list(LEGACY_EVENT_INSTANCE_LIMIT),
        'shared_palette_rgb': [list(color) for color in palette],
        'records': records,
    }
    manifest_path = project_dir / 'gba/reference/legacy_high_altitude_manifest.json'
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + '\n', encoding='utf-8')
    return manifest
