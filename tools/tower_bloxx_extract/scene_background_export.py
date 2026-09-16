from __future__ import annotations

import hashlib
import json
from dataclasses import dataclass
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
CONSTRUCTION_LAYER_WIDTH = 256
CONSTRUCTION_LAYER_HEIGHT = 512
SCENERY_CHUNK_CENTERS = (0, 256, 512)
SCENERY_MAX_SCROLL = 672
# House.<clinit> t[] palette, recovered directly from the canonical bytecode.
SKY_COLORS = (
    0xB2D6F2, 0x9AC8EA, 0x80BBE7, 0x66AFE4, 0x518EE4, 0x407ABE,
    0x1C5B96, 0x0C3F7C, 0x13306A, 0x34204C, 0x372C51, 0x2D4B4B,
    0x4A6742, 0x674723, 0x532733, 0x802A2B, 0x511A2F,
)
GROUND_EDGE = 0x463C14
GROUND_FILL = 0x1E190F



@dataclass(frozen=True)
class ConstructionSkyState:
    band: int
    current_color_index: int
    next_color_index: int
    horizon: int
    middle_color: int


@dataclass(frozen=True)
class HighAltitudeDecoration:
    x: int
    width: int
    world_y: int
    height: int
    color: int
    kind: int


class _JavaRandom:
    _multiplier = 0x5DEECE66D
    _addend = 0xB
    _mask = (1 << 48) - 1

    def __init__(self, seed: int) -> None:
        self._state = (seed ^ self._multiplier) & self._mask

    def next_int(self) -> int:
        self._state = (self._state * self._multiplier + self._addend) & self._mask
        value = self._state >> 16
        return value - (1 << 32) if value & 0x80000000 else value

    def source_mod(self, bound: int) -> int:
        value = self.next_int()
        remainder = value % bound if value >= 0 else -((-value) % bound)
        return abs(remainder)


def java_sky_color_index(band: int) -> int:
    if band < 0:
        band = 0
    if band <= 16:
        return band
    return 9 + ((band - 9) % 8)


def construction_sky_state(camera_y: int) -> ConstructionSkyState:
    ar = max(256 * VISIBLE_HEIGHT // 22, 2048)
    scaled = (2 * max(0, camera_y)) // 3
    horizon = (22 * (scaled % ar)) >> 8
    band = scaled // ar
    current_index = java_sky_color_index(band)
    next_index = java_sky_color_index(band + 1)
    return ConstructionSkyState(
        band=band,
        current_color_index=current_index,
        next_color_index=next_index,
        horizon=horizon,
        middle_color=_average_rgb(SKY_COLORS[current_index], SKY_COLORS[next_index]),
    )


def deterministic_high_altitude_decorations(width: int = VISIBLE_WIDTH) -> tuple[HighAltitudeDecoration, ...]:
    # House uses an unseeded java.util.Random, so positions legitimately vary
    # between launches.  Use a fixed seed for deterministic clean-room assets
    # while preserving House.v()'s exact ranges and call order.
    random = _JavaRandom(0x54424C4F5858)
    colors = (0x6FA7D0, 0x5CA0D1, 0x4F98CD)
    cell_width = width // 12
    result: list[HighAltitudeDecoration] = []
    for index in range(12):
        item_width = 5 + random.source_mod(5)
        x = index * cell_width - random.source_mod(item_width)
        world_y = 440 + random.source_mod(176)
        height = 11 + random.source_mod(11)
        color = colors[random.source_mod(3)]
        kind = random.source_mod(3)
        result.append(HighAltitudeDecoration(x, item_width, world_y, height, color, kind))
    return tuple(result)


def scaled_resource43_extent(jar_path: Path) -> int:
    with zipfile.ZipFile(Path(jar_path)) as jar:
        data = decode_resource_43(read_resource(jar, 43))
    return max(44 * (entry.y_start + entry.height) // 32 for entry in data.entries)

def _rgb(color: int) -> tuple[int, int, int, int]:
    return ((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF, 255)


def _average_rgb(left: int, right: int) -> int:
    red = (((left >> 16) & 0xFF) + ((right >> 16) & 0xFF)) >> 1
    green = (((left >> 8) & 0xFF) + ((right >> 8) & 0xFF)) >> 1
    blue = ((left & 0xFF) + (right & 0xFF)) >> 1
    return (red << 16) | (green << 8) | blue


def _draw_java_sky(image: Image.Image, camera_y: int = 512) -> None:
    state = construction_sky_state(camera_y)
    horizon = state.horizon
    left = SKY_COLORS[state.current_color_index]
    right = SKY_COLORS[state.next_color_index]
    middle = state.middle_color

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


def render_construction_background(jar_path: Path, camera_y: int = 512) -> Image.Image:
    jar_path = Path(jar_path)
    image = Image.new("RGBA", (VISIBLE_WIDTH, VISIBLE_HEIGHT), _rgb(SKY_COLORS[0]))
    _draw_java_sky(image, camera_y=camera_y)
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



def _next_sky_color_index(current_index: int) -> int:
    return 9 if current_index == 16 else current_index + 1


def render_construction_sky_layer(current_index: int) -> Image.Image:
    if current_index < 0 or current_index >= len(SKY_COLORS):
        raise ValueError("construction sky color index out of range")
    next_index = _next_sky_color_index(current_index)
    current = SKY_COLORS[current_index]
    following = SKY_COLORS[next_index]
    middle = _average_rgb(current, following)
    image = Image.new(
        "RGBA", (CONSTRUCTION_LAYER_WIDTH, CONSTRUCTION_LAYER_HEIGHT), _rgb(current)
    )
    draw = ImageDraw.Draw(image)
    transition_y = CONSTRUCTION_LAYER_HEIGHT // 2
    draw.rectangle(
        (0, 0, CONSTRUCTION_LAYER_WIDTH - 1, transition_y - 1), fill=_rgb(following)
    )

    # House chooses a random center within the middle half of the screen when
    # the color band changes.  The Java RNG is unseeded; the clean-room port
    # uses the deterministic midpoint while preserving the exact 7px motif.
    visible_left = (CONSTRUCTION_LAYER_WIDTH - VISIBLE_WIDTH) // 2
    center = visible_left + VISIBLE_WIDTH // 2
    draw.rectangle(
        (visible_left, transition_y - 7, center - 1, transition_y - 1), fill=_rgb(middle)
    )
    draw.rectangle(
        (center, transition_y - 1, visible_left + VISIBLE_WIDTH - 1, transition_y + 5),
        fill=_rgb(middle),
    )
    for half_width, y_delta, height in (
        (24, -2, 3), (18, -3, 5), (16, -5, 9), (15, -6, 11), (13, -7, 13)
    ):
        draw.rectangle(
            (
                center - half_width,
                transition_y + y_delta,
                center + half_width - 1,
                transition_y + y_delta + height - 1,
            ),
            fill=_rgb(middle),
        )
    return image


def _opening_resource43_geometry(entry) -> tuple[int, int, int, int]:
    x = VISIBLE_WIDTH * entry.x_ref // 176
    width = max(1, VISIBLE_WIDTH * (entry.x_ref + entry.width_ref) // 176 - x)
    y_start = 44 * entry.y_start // 32
    height = max(1, 44 * (entry.y_start + entry.height) // 32 - y_start)
    y = VISIBLE_HEIGHT // 2 - y_start - height + _construction_ground_line()
    return x, y, width, height


def _draw_procedural_skyline(
    image: Image.Image, chunk_center: int, decorations: tuple[HighAltitudeDecoration, ...]
) -> None:
    draw = ImageDraw.Draw(image)
    x_offset = (CONSTRUCTION_LAYER_WIDTH - VISIBLE_WIDTH) // 2
    y_offset = (CONSTRUCTION_LAYER_HEIGHT - VISIBLE_HEIGHT) // 2
    opening_ground_bottom = VISIBLE_HEIGHT // 2 + (22 * 512 >> 8)  # House.w + camera pixel offset = 124.
    for decoration in decorations:
        top = opening_ground_bottom - decoration.world_y + chunk_center
        bottom = opening_ground_bottom + chunk_center
        left = x_offset + decoration.x
        draw.rectangle(
            (left, y_offset + top, left + decoration.width - 1, y_offset + bottom - 1),
            fill=_rgb(decoration.color),
        )
        if decoration.kind == 1:
            antenna_x = left + decoration.width // 2
            draw.rectangle(
                (
                    antenna_x,
                    y_offset + top - decoration.height,
                    antenna_x + 1,
                    y_offset + top - 1,
                ),
                fill=_rgb(decoration.color),
            )
        elif decoration.kind == 2:
            cap_height = decoration.width - decoration.width // 5
            cap_x = left + decoration.width // 10
            draw.rectangle(
                (
                    cap_x,
                    y_offset + top - 11,
                    cap_x + decoration.width - 1,
                    y_offset + top - 12 + cap_height,
                ),
                fill=_rgb(decoration.color),
            )


def render_construction_scenery_chunk(
    jar_path: Path,
    chunk_center: int,
    decorations: tuple[HighAltitudeDecoration, ...] | None = None,
) -> Image.Image:
    if chunk_center not in SCENERY_CHUNK_CENTERS:
        raise ValueError("invalid scenery chunk center")
    if decorations is None:
        decorations = deterministic_high_altitude_decorations()

    jar_path = Path(jar_path)
    image = Image.new(
        "RGBA", (CONSTRUCTION_LAYER_WIDTH, CONSTRUCTION_LAYER_HEIGHT), (0, 0, 0, 0)
    )
    x_offset = (CONSTRUCTION_LAYER_WIDTH - VISIBLE_WIDTH) // 2
    y_offset = (CONSTRUCTION_LAYER_HEIGHT - VISIBLE_HEIGHT) // 2
    _draw_procedural_skyline(image, chunk_center, decorations)

    with zipfile.ZipFile(jar_path) as jar:
        skyline = decode_resource_43(read_resource(jar, 43))
        foreground = [
            Image.open(BytesIO(read_resource(jar, resource_id))).convert("RGBA")
            for resource_id in (31, 32, 33, 34)
        ]

    draw = ImageDraw.Draw(image)
    for entry in skyline.entries:
        x, y, width, height = _opening_resource43_geometry(entry)
        y += chunk_center
        draw.rectangle(
            (
                x_offset + x,
                y_offset + y,
                x_offset + x + width - 1,
                y_offset + y + height - 1,
            ),
            fill=_rgb(skyline.int_values[entry.kind]),
        )

    site, left_repeat, right_repeat, tree = foreground
    ground_line = _construction_ground_line() + chunk_center
    center = VISIBLE_WIDTH // 2
    site_left = center - site.width // 2
    tree_center_x = center + site.width // 2
    _paste_png(
        image,
        tree,
        x_offset + tree_center_x - tree.width // 2,
        y_offset + ground_line - tree.height + 7,
    )
    _paste_png(image, site, x_offset + site_left, y_offset + ground_line)
    x = site_left - left_repeat.width
    while x > -left_repeat.width:
        _paste_png(image, left_repeat, x_offset + x, y_offset + ground_line)
        x -= left_repeat.width
    x = center + site.width // 2
    while x < VISIBLE_WIDTH:
        _paste_png(image, right_repeat, x_offset + x, y_offset + ground_line)
        x += right_repeat.width

    ground_y = ground_line + site.height
    if y_offset + ground_y < CONSTRUCTION_LAYER_HEIGHT:
        draw.rectangle(
            (
                x_offset,
                y_offset + ground_y,
                x_offset + VISIBLE_WIDTH - 1,
                min(CONSTRUCTION_LAYER_HEIGHT - 1, y_offset + ground_y + 1),
            ),
            fill=_rgb(GROUND_EDGE),
        )
        if y_offset + ground_y + 2 < CONSTRUCTION_LAYER_HEIGHT:
            draw.rectangle(
                (
                    x_offset,
                    y_offset + ground_y + 2,
                    x_offset + VISIBLE_WIDTH - 1,
                    CONSTRUCTION_LAYER_HEIGHT - 1,
                ),
                fill=_rgb(GROUND_FILL),
            )
    return image


def _write_construction_background_header(
    project_dir: Path, decorations: tuple[HighAltitudeDecoration, ...]
) -> Path:
    generated_dir = Path(project_dir) / "gba" / "include" / "generated"
    generated_dir.mkdir(parents=True, exist_ok=True)
    path = generated_dir / "construction_background_data.h"
    records = "\n".join(
        "    ConstructionBackgroundDecoration{" +
        f"{item.x}, {item.width}, {item.world_y}, {item.height}, 0x{item.color:06X}u, {item.kind}" +
        "},"
        for item in decorations
    )
    centers = ", ".join(str(value) for value in SCENERY_CHUNK_CENTERS)
    path.write_text(
        "#ifndef GENERATED_CONSTRUCTION_BACKGROUND_DATA_H\n"
        "#define GENERATED_CONSTRUCTION_BACKGROUND_DATA_H\n\n"
        "#include <cstdint>\n\n"
        "namespace tb::generated\n{\n"
        "struct ConstructionBackgroundDecoration\n{\n"
        "    int x;\n    int width;\n    int world_y;\n    int roof_height;\n"
        "    uint32_t color;\n    int kind;\n};\n\n"
        f"inline constexpr int construction_scenery_chunk_centers[] = {{{centers}}};\n"
        f"inline constexpr int construction_scenery_max_scroll = {SCENERY_MAX_SCROLL};\n"
        "inline constexpr ConstructionBackgroundDecoration construction_background_decorations[] = {\n"
        f"{records}\n"
        "};\n}\n\n#endif\n",
        encoding="utf-8",
    )
    return path


def _write_high_altitude_blink_sprite(project_dir: Path) -> tuple[Path, Path]:
    ui_dir = Path(project_dir) / "gba" / "graphics" / "ui"
    ui_dir.mkdir(parents=True, exist_ok=True)
    image = Image.new("RGBA", (8, 8), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.rectangle((3, 3, 4, 4), fill=(255, 0, 0, 255))
    indices, palette = _indexed_transparent(image, bpp=4)
    bmp_path = ui_dir / "construction_high_blink_p0.bmp"
    json_path = ui_dir / "construction_high_blink_p0.json"
    _write_indexed_bmp(bmp_path, 8, 8, indices, palette, 4)
    json_path.write_text(
        json.dumps({"bpp_mode": "bpp_4", "type": "sprite"}, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    return bmp_path, json_path

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


def _indexed_background(
    image: Image.Image, *, reserve_transparent_index: bool = False
) -> tuple[bytes, tuple[int, ...]]:
    rgba = image.convert("RGBA")
    # Butano treats regular-BG palette index 0 as transparent.  City screens
    # are fully opaque Java2D compositions, so reserve slot 0 and place every
    # visible source color at index >= 1.  Without this, the very common source
    # white becomes index 0 and the bottom message panel shows the blue
    # backdrop through it instead of white.
    palette: list[int] = [0] if reserve_transparent_index else []
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



def _indexed_transparent(image: Image.Image, bpp: int = 8) -> tuple[bytes, tuple[int, ...]]:
    rgba = image.convert("RGBA")
    palette: list[int] = [0]
    mapping: dict[int, int] = {}
    indices = bytearray(rgba.width * rgba.height)
    max_colors = 16 if bpp == 4 else 256
    for index, (red, green, blue, alpha) in enumerate(rgba.get_flattened_data()):
        if alpha == 0:
            indices[index] = 0
            continue
        value = (red >> 3) | ((green >> 3) << 5) | ((blue >> 3) << 10)
        palette_index = mapping.get(value)
        if palette_index is None:
            if len(palette) >= max_colors:
                raise ValueError("transparent asset exceeds palette capacity")
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
    jar_path = Path(jar_path)
    graphics_dir = project_dir / "gba" / "graphics" / "backgrounds"
    reference_dir = project_dir / "gba" / "reference"
    graphics_dir.mkdir(parents=True, exist_ok=True)
    reference_dir.mkdir(parents=True, exist_ok=True)

    for legacy_name in (
        "city_bg.bmp", "city_bg.json",
        "construction_bg.bmp", "construction_bg.json",
        "construction_bg_b1.bmp", "construction_bg_b1.json",
        "construction_bg_b2.bmp", "construction_bg_b2.json",
        "construction_bg_b3.bmp", "construction_bg_b3.json",
    ):
        legacy_path = graphics_dir / legacy_name
        if legacy_path.exists():
            legacy_path.unlink()

    decorations = deterministic_high_altitude_decorations()
    assets: dict[str, Image.Image] = {}
    for index in range(len(SKY_COLORS)):
        assets[f"construction_sky_{index:02d}"] = render_construction_sky_layer(index)
    for index, center in enumerate(SCENERY_CHUNK_CENTERS):
        assets[f"construction_scenery_{index}"] = render_construction_scenery_chunk(
            jar_path, center, decorations
        )
    assets.update({
        "city_bg_theme_0": render_city_background(0),
        "city_bg_theme_1": render_city_background(1),
        "city_bg_theme_2": render_city_background(2),
        "city_bg_theme_3": render_city_background(3),
        "menu_bg": render_menu_background(),
    })

    files: list[dict[str, object]] = []
    for name, image in assets.items():
        construction_layer = name.startswith("construction_sky_") or name.startswith("construction_scenery_")
        if construction_layer:
            output = image
        else:
            output = _pad_visible(image)

        if name.startswith("construction_scenery_"):
            indices, palette = _indexed_transparent(output, bpp=8)
        else:
            indices, palette = _indexed_background(
                output, reserve_transparent_index=name.startswith("city_bg_theme_")
            )
        bmp_path = graphics_dir / f"{name}.bmp"
        json_path = graphics_dir / f"{name}.json"
        _write_indexed_bmp(bmp_path, output.width, output.height, indices, palette, 8)
        json_path.write_text(
            json.dumps({"bpp_mode": "bpp_8", "type": "regular_bg"}, sort_keys=True) + "\n"
        )
        for path in (bmp_path, json_path):
            files.append({
                "path": path.relative_to(project_dir).as_posix(),
                "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                "size": path.stat().st_size,
            })

    header_path = _write_construction_background_header(project_dir, decorations)
    blink_paths = _write_high_altitude_blink_sprite(project_dir)
    for path in (header_path, *blink_paths):
        files.append({
            "path": path.relative_to(project_dir).as_posix(),
            "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
            "size": path.stat().st_size,
        })

    with zipfile.ZipFile(jar_path) as jar:
        resource43 = decode_resource_43(read_resource(jar, 43))
    manifest = {
        "visible_size": [VISIBLE_WIDTH, VISIBLE_HEIGHT],
        "asset_size": [ASSET_SIZE, ASSET_SIZE],
        "construction_layer_size": [CONSTRUCTION_LAYER_WIDTH, CONSTRUCTION_LAYER_HEIGHT],
        "assets": list(assets),
        "construction_ground_line": _construction_ground_line(),
        "resource43_entry_count": len(resource43.entries),
        "resource43_scaled_extent": max(
            44 * (entry.y_start + entry.height) // 32 for entry in resource43.entries
        ),
        "scenery_chunk_centers": list(SCENERY_CHUNK_CENTERS),
        "scenery_max_scroll": SCENERY_MAX_SCROLL,
        "high_altitude_decoration_count": len(decorations),
        "files": sorted(files, key=lambda record: str(record["path"])),
    }
    (reference_dir / "scene_backgrounds_manifest.json").write_text(
        json.dumps(manifest, sort_keys=True, indent=2) + "\n", encoding="utf-8"
    )
    return manifest

