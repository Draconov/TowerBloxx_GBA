from __future__ import annotations

import hashlib
import json
from io import BytesIO
from pathlib import Path
import zipfile

from PIL import Image, ImageDraw

from .binary43 import decode_resource_43
from .gba_project_export import _write_indexed_bmp
from .m3g_render import house_gameplay_camera_distance, project_camera_point, runtime_camera
from .resources import read_resource

VISIBLE_WIDTH = 240
VISIBLE_HEIGHT = 160
ASSET_SIZE = 256
# House.<clinit> t[] palette, recovered directly from the canonical bytecode.
SKY_COLORS = (
    0xB2D6F2, 0x9AC8EA, 0x80BBE7, 0x66AFE4, 0x518EE4, 0x407ABE,
    0x1C5B96, 0x0C3F7C, 0x13306A, 0x34204C, 0x372C51, 0x2D4B4B,
    0x4A6742, 0x674723, 0x532733, 0x802A2B, 0x511A2F,
)
GROUND_EDGE = 0x463C14
GROUND_FILL = 0x1E190F


def _rgb(color: int) -> tuple[int, int, int, int]:
    return ((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF, 255)


def _average_rgb(left: int, right: int) -> int:
    red = (((left >> 16) & 0xFF) + ((right >> 16) & 0xFF)) >> 1
    green = (((left >> 8) & 0xFF) + ((right >> 8) & 0xFF)) >> 1
    blue = ((left & 0xFF) + (right & 0xFF)) >> 1
    return (red << 16) | (green << 8) | blue


def _draw_java_sky(image: Image.Image, camera_y: int = 512) -> None:
    # House.o(): I = 256*height/22; House.m(): ar=max(I,2048).
    ar = max(256 * VISIBLE_HEIGHT // 22, 2048)
    scaled = (2 * camera_y) // 3
    horizon = (22 * (scaled % ar)) >> 8
    band = scaled // ar
    left = SKY_COLORS[max(0, min(9, band))]
    right = SKY_COLORS[max(0, min(9, band + 1))]
    middle = _average_rgb(left, right)

    draw = ImageDraw.Draw(image)
    draw.rectangle((0, 0, VISIBLE_WIDTH - 1, VISIBLE_HEIGHT - 1), fill=_rgb(left))
    if horizon > 0:
        draw.rectangle((0, 0, VISIBLE_WIDTH - 1, horizon - 1), fill=_rgb(right))

    # House.a(Graphics,int,boolean) uses a random center within the middle half
    # of the display for its 7px transition motif. The exact RNG is unseeded in
    # Java; use the deterministic midpoint for the clean-room build.
    center = VISIBLE_WIDTH // 2
    draw.rectangle((0, horizon - 7, center - 1, horizon - 1), fill=_rgb(middle))
    draw.rectangle((center, horizon - 1, VISIBLE_WIDTH - 1, horizon + 5), fill=_rgb(middle))
    for half_width, y_delta, height in ((24, -2, 3), (18, -3, 5), (16, -5, 9), (15, -6, 11), (13, -7, 13)):
        draw.rectangle(
            (center - half_width, horizon + y_delta,
             center + half_width - 1, horizon + y_delta + height - 1),
            fill=_rgb(middle),
        )


def render_menu_background() -> Image.Image:
    """Render the sky composition visible behind the v1.5.22 main menu.

    The reference JAR capture lands on House sky band 1. Solving the recovered
    House.a(Graphics, int, boolean) horizon math for the observed 11px cap gives
    camera_y=3264, so this uses the same recovered renderer rather than a
    screenshot-derived bitmap.
    """
    image = Image.new("RGBA", (VISIBLE_WIDTH, VISIBLE_HEIGHT), _rgb(SKY_COLORS[1]))
    _draw_java_sky(image, camera_y=3264)
    return image


def _construction_ground_line() -> int:
    camera_z = house_gameplay_camera_distance(VISIBLE_WIDTH, VISIBLE_HEIGHT)
    camera = runtime_camera(VISIBLE_WIDTH, VISIBLE_HEIGHT, base_fov=55.0)
    # House.h() projects world point (0,-19,-80) while the camera is at
    # (0, cameraY, aA); resource 31 is then placed at projectedY-height+7.
    _x, screen_y, _depth = project_camera_point(
        camera, (0.0, -19.0 - 512.0, -80.0 - float(camera_z))
    )
    return int(screen_y) - 27 + 7


def _draw_resource43_skyline(image: Image.Image, raw: bytes, ground_line: int) -> None:
    data = decode_resource_43(raw)
    draw = ImageDraw.Draw(image)
    half_height = VISIBLE_HEIGHT // 2  # House.w
    for entry in data.entries:
        x = VISIBLE_WIDTH * entry.x_ref // 176
        width = max(1, VISIBLE_WIDTH * (entry.x_ref + entry.width_ref) // 176 - x)
        y_start = 44 * entry.y_start // 32
        height = max(1, 44 * (entry.y_start + entry.height) // 32 - y_start)
        y = half_height - y_start - height + ground_line
        if y + height < 0:
            # Entries are ordered by source y; Java returns here.
            break
        if y > VISIBLE_HEIGHT:
            continue
        color = data.int_values[entry.kind]
        draw.rectangle((x, y, x + width - 1, y + height - 1), fill=_rgb(color))


def _paste_png(image: Image.Image, png: Image.Image, x: int, y: int) -> None:
    image.alpha_composite(png.convert("RGBA"), (x, y))


def render_construction_background(jar_path: Path) -> Image.Image:
    jar_path = Path(jar_path)
    image = Image.new("RGBA", (VISIBLE_WIDTH, VISIBLE_HEIGHT), _rgb(SKY_COLORS[0]))
    _draw_java_sky(image)
    ground_line = _construction_ground_line()

    with zipfile.ZipFile(jar_path) as jar:
        _draw_resource43_skyline(image, read_resource(jar, 43), ground_line)
        foreground = [
            Image.open(BytesIO(read_resource(jar, resource_id))).convert("RGBA")
            for resource_id in (31, 32, 33, 34)
        ]

    site, left_repeat, right_repeat, tree = foreground
    center = VISIBLE_WIDTH // 2
    site_left = center - site.width // 2

    # House.h() resource 34, anchor HCENTER|TOP.
    tree_center_x = center + site.width // 2
    _paste_png(image, tree, tree_center_x - tree.width // 2, ground_line - tree.height + 7)

    # Resource 31 starts in the middle; resource 32 tiles to the left and 33
    # tiles to the right exactly like the Java draw loop.
    _paste_png(image, site, site_left, ground_line)
    x = site_left - left_repeat.width
    while x > -left_repeat.width:
        _paste_png(image, left_repeat, x, ground_line)
        x -= left_repeat.width
    x = center + site.width // 2
    while x < VISIBLE_WIDTH:
        _paste_png(image, right_repeat, x, ground_line)
        x += right_repeat.width

    draw = ImageDraw.Draw(image)
    ground_y = ground_line + site.height
    if ground_y < VISIBLE_HEIGHT:
        draw.rectangle((0, ground_y, VISIBLE_WIDTH - 1, min(VISIBLE_HEIGHT - 1, ground_y + 1)), fill=_rgb(GROUND_EDGE))
        if ground_y + 2 < VISIBLE_HEIGHT:
            draw.rectangle((0, ground_y + 2, VISIBLE_WIDTH - 1, VISIBLE_HEIGHT - 1), fill=_rgb(GROUND_FILL))
    return image


def render_city_background() -> Image.Image:
    """Render the empty 5x5 City Grid field behind the saved tower sprites.

    The original m.a(Graphics,boolean) is pure Java 2D. This captures its
    green field / road-grid presentation in the same GBA coordinates already
    used by BuildCityScene, rather than leaving an empty sky backdrop.
    """
    image = Image.new("RGBA", (VISIBLE_WIDTH, VISIBLE_HEIGHT), (175, 229, 255, 255))
    draw = ImageDraw.Draw(image)

    # Darker lower city field and road frame, using colors recovered from m's
    # static palette (signed Java ints masked to RGB).
    draw.rectangle((0, 32, VISIBLE_WIDTH - 1, VISIBLE_HEIGHT - 1), fill=(173, 153, 99, 255))
    draw.rectangle((56, 38, 184, 132), fill=(42, 67, 20, 255))
    draw.rectangle((61, 43, 179, 127), fill=(86, 117, 48, 255))

    grid_left = 120 - 34
    grid_top = 80 - 25
    spacing = 17
    for row in range(5):
        for column in range(5):
            cx = grid_left + column * spacing
            cy = grid_top + row * spacing
            # 15x15 lots leave the original narrow road gaps between sectors.
            fill = (104, 139, 62, 255) if (row + column) & 1 else (112, 147, 68, 255)
            draw.rectangle((cx - 7, cy - 7, cx + 7, cy + 7), fill=fill)
            draw.line((cx - 7, cy - 7, cx + 7, cy - 7), fill=(188, 184, 137, 255))
            draw.line((cx - 7, cy + 7, cx + 7, cy + 7), fill=(48, 73, 27, 255))

    # Demolishing lot at the left of row 4, matching the current controller's
    # column=-1 selection position.
    demo_x = grid_left - 31
    demo_y = grid_top + 4 * spacing
    draw.rectangle((demo_x - 8, demo_y - 8, demo_x + 8, demo_y + 8), fill=(88, 88, 88, 255))
    draw.line((demo_x - 6, demo_y - 6, demo_x + 6, demo_y + 6), fill=(180, 40, 32, 255), width=2)
    draw.line((demo_x + 6, demo_y - 6, demo_x - 6, demo_y + 6), fill=(180, 40, 32, 255), width=2)
    return image


def _indexed_background(image: Image.Image) -> tuple[bytes, tuple[int, ...]]:
    rgba = image.convert("RGBA")
    palette: list[int] = []
    mapping: dict[int, int] = {}
    indices = bytearray(rgba.width * rgba.height)
    for index, (red, green, blue, _alpha) in enumerate(rgba.get_flattened_data()):
        value = (red >> 3) | ((green >> 3) << 5) | ((blue >> 3) << 10)
        palette_index = mapping.get(value)
        if palette_index is None:
            if len(palette) >= 256:
                raise ValueError("background exceeds 256 GBA colors")
            palette_index = len(palette)
            mapping[value] = palette_index
            palette.append(value)
        indices[index] = palette_index
    return bytes(indices), tuple(palette)


def _pad_visible(image: Image.Image) -> Image.Image:
    canvas = Image.new("RGBA", (ASSET_SIZE, ASSET_SIZE), image.getpixel((0, 0)))
    canvas.alpha_composite(image, ((ASSET_SIZE - VISIBLE_WIDTH) // 2, (ASSET_SIZE - VISIBLE_HEIGHT) // 2))
    return canvas


def export_scene_backgrounds(jar_path: Path, project_dir: Path) -> dict[str, object]:
    project_dir = Path(project_dir)
    graphics_dir = project_dir / "gba" / "graphics" / "backgrounds"
    reference_dir = project_dir / "gba" / "reference"
    graphics_dir.mkdir(parents=True, exist_ok=True)
    reference_dir.mkdir(parents=True, exist_ok=True)

    assets = {
        "construction_bg": render_construction_background(jar_path),
        "city_bg": render_city_background(),
        "menu_bg": render_menu_background(),
    }
    files: list[dict[str, object]] = []
    for name, visible in assets.items():
        padded = _pad_visible(visible)
        indices, palette = _indexed_background(padded)
        bmp_path = graphics_dir / f"{name}.bmp"
        json_path = graphics_dir / f"{name}.json"
        _write_indexed_bmp(bmp_path, ASSET_SIZE, ASSET_SIZE, indices, palette, 8)
        json_path.write_text(json.dumps({"bpp_mode": "bpp_8", "type": "regular_bg"}, sort_keys=True) + "\n")
        for path in (bmp_path, json_path):
            files.append({
                "path": path.relative_to(project_dir).as_posix(),
                "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                "size": path.stat().st_size,
            })

    manifest = {
        "visible_size": [VISIBLE_WIDTH, VISIBLE_HEIGHT],
        "asset_size": [ASSET_SIZE, ASSET_SIZE],
        "assets": list(assets),
        "construction_ground_line": _construction_ground_line(),
        "files": sorted(files, key=lambda record: str(record["path"])),
    }
    (reference_dir / "scene_backgrounds_manifest.json").write_text(
        json.dumps(manifest, sort_keys=True, indent=2) + "\n", encoding="utf-8"
    )
    return manifest
