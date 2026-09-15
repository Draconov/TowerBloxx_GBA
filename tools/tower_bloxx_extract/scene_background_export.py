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
    screenshot-derived bitmap.  The original menu sky also carries a pair of
    pale cloud banks that sit behind the logo/worker scene, so keep those in
    the clean-room backdrop instead of leaving the menu as a flat blue field.
    """
    image = Image.new("RGBA", (VISIBLE_WIDTH, VISIBLE_HEIGHT), _rgb(SKY_COLORS[1]))
    _draw_java_sky(image, camera_y=3264)

    draw = ImageDraw.Draw(image)
    cloud_color = (170, 204, 230, 255)
    for x, y, width, height in (
        (8, 26, 78, 30),
        (60, 29, 74, 26),
        (110, 25, 62, 28),
        (156, 31, 76, 24),
    ):
        draw.rounded_rectangle((x, y, x + width, y + height), radius=8, fill=cloud_color)

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


def render_city_background(theme_index: int = 0) -> Image.Image:
    """Render the recovered 240x160 Build City compositor base.

    m.a(Graphics, boolean) draws this screen almost entirely with Java 2D:
    a 13px status bar, a #AFE5FF -> #588CFF playfield gradient, an 88x88
    road/grid panel, the four-slot tower selector and a 23px bottom message
    panel.  Dynamic digits, tower previews, saved buildings and effects remain
    sprites in BuildCityScene.
    """
    image = Image.new("RGBA", (VISIBLE_WIDTH, VISIBLE_HEIGHT), (255, 255, 255, 255))
    draw = ImageDraw.Draw(image)

    # m.a[] = {175,229,255,-87,-89,0}.  Java integer division truncates
    # toward zero; all terms here are non-negative before adding the deltas.
    gradient_last = VISIBLE_HEIGHT - 22
    denominator = gradient_last - 13
    for y in range(13, gradient_last + 1):
        t = y - 13
        red = 175 - (87 * t) // denominator
        green = 229 - (89 * t) // denominator
        blue = 255
        draw.line((0, y, VISIBLE_WIDTH - 1, y), fill=(red, green, blue, 255))

    # Top status band: m.b[] = #AD9C83, #231E14, #231E14, #EBE1E1, #140C0C.
    top_colors = ((173, 156, 131), (35, 30, 20), (35, 30, 20), (235, 225, 225), (20, 12, 12))
    y = 0
    draw.rectangle((3, y, VISIBLE_WIDTH - 4, y + 10), fill=(*top_colors[0], 255))
    y += 11
    for color in top_colors[1:]:
        draw.line((3, y, VISIBLE_WIDTH - 4, y), fill=(*color, 255))
        y += 1

    # Bottom message panel starts at h-23 and overwrites the gradient there.
    bottom_y = VISIBLE_HEIGHT - 23
    draw.line((3, bottom_y, VISIBLE_WIDTH - 4, bottom_y), fill=(20, 12, 12, 255))
    draw.line((3, bottom_y + 1, VISIBLE_WIDTH - 4, bottom_y + 1), fill=(125, 110, 110, 255))
    draw.rectangle((3, bottom_y + 2, VISIBLE_WIDTH - 7, VISIBLE_HEIGHT - 1), fill=(255, 255, 255, 255))

    # Exact 240x160 specialization of m's board positioning formula.
    board_x = ((VISIBLE_WIDTH - 19 - 7 - 88) >> 1) + 19 + 7  # 89
    board_y = 15 + ((VISIBLE_HEIGHT - 15 - 23 - 88 + 1) >> 1)  # 32
    draw.rectangle((board_x, board_y, board_x + 87, board_y + 87), fill=(255, 255, 255, 255))
    draw.rectangle((board_x + 1, board_y + 1, board_x + 86, board_y + 86), fill=(64, 64, 64, 255))

    # m.e/m.d are four source lot palette themes selected by the same
    # milestone tiers as the building unlocks: 0, 3, 6, 10.
    lot_outer_colors = ((0x78, 0xBC, 0x28), (0x7F, 0xAF, 0x46),
                        (0x84, 0xA5, 0x5D), (0x8A, 0x9C, 0x74))
    lot_inner_colors = ((0x43, 0x78, 0x17), (0x50, 0x6E, 0x37),
                        (0x58, 0x69, 0x50), (0x62, 0x61, 0x6A))
    if theme_index < 0:
        theme_index = 0
    elif theme_index > 3:
        theme_index = 3
    lot_outer = (*lot_outer_colors[theme_index], 255)
    lot_inner = (*lot_inner_colors[theme_index], 255)
    for row in range(5):
        for column in range(5):
            x = board_x + 3 + column * 17
            y = board_y + 3 + row * 17
            draw.rectangle((x, y, x + 13, y + 13), fill=lot_outer)
            draw.rectangle((x + 1, y + 1, x + 12, y + 12), fill=lot_inner)
            draw.rectangle((x + 2, y + 2, x + 11, y + 11), fill=lot_outer)

    # Dashed road separators from m.a(Graphics,boolean), color #929292.
    road = (146, 146, 146, 255)
    for separator in range(4):
        x = board_x + 3 + 15 + separator * 17
        for y0 in range(board_y + 4, board_y + 84, 6):
            draw.line((x, y0, x, min(y0 + 2, board_y + 84)), fill=road)
        y = board_y + 3 + 15 + separator * 17
        for x0 in range(board_x + 4, board_x + 84, 6):
            draw.line((x0, y, min(x0 + 2, board_x + 84), y), fill=road)

    # Browse-mode four-slot selector recovered from bytecode after x -= 26.
    selector_x = board_x - 26
    selector_y = board_y + 2
    draw.rectangle((selector_x, selector_y, selector_x + 18, selector_y + 66), fill=(125, 110, 110, 255))
    draw.rectangle((selector_x + 1, selector_y + 1, selector_x + 17, selector_y + 65), fill=(255, 255, 255, 255))
    for slot in range(4):
        slot_y = selector_y + 2 + slot * 16
        draw.rectangle((selector_x + 2, slot_y, selector_x + 16, slot_y + 14), fill=(172, 172, 172, 255))

    # Browse-mode top-right HUD pair from m.a(Graphics,boolean): x starts at
    # width-60, the icon consumes 9px, then two 24x9 outlined value boxes.
    status_x = VISIBLE_WIDTH - 60 + 9
    for box in range(2):
        x = status_x + box * 24
        draw.rectangle((x, 1, x + 23, 9), fill=(199, 191, 178, 255))  # #C7BFB2
        draw.rectangle((x + 1, 2, x + 22, 8), fill=(173, 156, 131, 255))  # #AD9C83
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

    # Fix 15.2 replaces the old single city_bg with the four recovered source themes.
    for legacy_name in ("city_bg.bmp", "city_bg.json"):
        legacy_path = graphics_dir / legacy_name
        if legacy_path.exists():
            legacy_path.unlink()

    assets = {
        "construction_bg": render_construction_background(jar_path),
        "city_bg_theme_0": render_city_background(0),
        "city_bg_theme_1": render_city_background(1),
        "city_bg_theme_2": render_city_background(2),
        "city_bg_theme_3": render_city_background(3),
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
