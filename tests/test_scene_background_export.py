from __future__ import annotations

import hashlib
import json
from pathlib import Path

from PIL import Image

from tower_bloxx_extract.scene_background_export import (
    SKY_COLORS,
    construction_sky_state,
    deterministic_high_altitude_decorations,
    export_scene_backgrounds,
    java_sky_color_index,
    render_city_background,
    render_construction_background,
    render_menu_background,
    scaled_resource43_extent,
)




def test_gameplay_sky_uses_all_17_colors_then_cycles_9_through_16() -> None:
    assert [java_sky_color_index(index) for index in range(17)] == list(range(17))
    assert [java_sky_color_index(index) for index in range(17, 25)] == list(range(9, 17))
    assert [java_sky_color_index(index) for index in range(25, 33)] == list(range(9, 17))


def test_gameplay_sky_horizon_and_band_follow_house_math() -> None:
    opening = construction_sky_state(512)
    assert opening.band == 0
    assert opening.current_color_index == 0
    assert opening.next_color_index == 1
    assert opening.horizon == 29

    threshold = construction_sky_state(3072)
    assert threshold.band == 1
    assert threshold.current_color_index == 1
    assert threshold.next_color_index == 2
    assert threshold.horizon == 0

    band_16 = construction_sky_state(16 * 3072)
    assert band_16.current_color_index == 16
    assert band_16.next_color_index == 9

    band_17 = construction_sky_state(17 * 3072)
    assert band_17.current_color_index == 9
    assert band_17.next_color_index == 10


def test_resource43_full_scaled_extent_is_preserved(tower_bloxx_jar: Path) -> None:
    assert scaled_resource43_extent(tower_bloxx_jar) == 621


def test_high_altitude_decorations_match_house_ranges_and_are_deterministic() -> None:
    first = deterministic_high_altitude_decorations(240)
    second = deterministic_high_altitude_decorations(240)
    assert first == second
    assert len(first) == 12
    allowed_colors = {0x6FA7D0, 0x5CA0D1, 0x4F98CD}
    for index, decoration in enumerate(first):
        assert 5 <= decoration.width <= 9
        assert 440 <= decoration.world_y <= 615
        assert 11 <= decoration.height <= 21
        assert decoration.color in allowed_colors
        assert decoration.kind in {0, 1, 2}
        cell_width = 240 // 12
        assert index * cell_width - decoration.width + 1 <= decoration.x <= index * cell_width

def test_menu_background_matches_reference_jar_sky_band() -> None:
    # House.<clinit> t[] recovered from the canonical v1.5.22 bytecode.
    assert SKY_COLORS == (
        0xB2D6F2, 0x9AC8EA, 0x80BBE7, 0x66AFE4, 0x518EE4, 0x407ABE,
        0x1C5B96, 0x0C3F7C, 0x13306A, 0x34204C, 0x372C51, 0x2D4B4B,
        0x4A6742, 0x674723, 0x532733, 0x802A2B, 0x511A2F,
    )

    image = render_menu_background()
    assert image.size == (240, 160)
    assert image.mode == "RGBA"
    # The captured JAR menu is sky band 1 with an 11px band-2 cap.
    assert image.getpixel((0, 0))[:3] == (0x80, 0xBB, 0xE7)
    assert image.getpixel((0, 20))[:3] == (0x9A, 0xC8, 0xEA)

    # The original menu also has pale cloud banks behind the tower/logo scene.
    assert image.getpixel((20, 40))[:3] == (170, 204, 230)
    assert image.getpixel((180, 42))[:3] == (170, 204, 230)


def test_construction_background_uses_recovered_nokia_layers(tower_bloxx_jar: Path) -> None:
    image = render_construction_background(tower_bloxx_jar)
    assert image.size == (240, 160)
    assert image.mode == "RGBA"

    # Pin the complete canonical opening composite. Resource 43 intentionally
    # covers much of the raw sky at the top of the frame, so sampling (0, 0)
    # as a "sky pixel" is incorrect; the full RGBA hash proves sky + skyline
    # + resources 31-34 + ground are composed in the recovered order.
    assert hashlib.sha256(image.tobytes()).hexdigest() == (
        "82d1efb49022f76b998a8d7de6010f5b0c17909e0054881daf6c3ee5aba686d9"
    )

    # House.h draws the construction foreground at the recovered projected
    # ground line around y=105 and then its brown ground fill below it.
    assert image.getpixel((10, 150))[:3] == (30, 25, 15)
    # Resource 31 fence/site strip contributes a non-sky pixel here.
    assert image.getpixel((82, 109))[:3] != (178, 214, 242)


def test_city_background_matches_recovered_build_city_compositor() -> None:
    image = render_city_background(0)
    assert image.size == (240, 160)
    assert image.mode == "RGBA"

    # m.a(Graphics,boolean): the playfield is the exact #AFE5FF -> #588CFF
    # vertical gradient between y=13 and y=137 on a 240x160 viewport.
    # y=13/14 are overwritten by the recovered top-band border.
    assert image.getpixel((120, 15))[:3] == (174, 228, 255)
    assert image.getpixel((120, 136))[:3] == (90, 142, 255)

    # Source top status bar and bottom white instruction panel are structural UI,
    # not the tan placeholder field from Fix 6.
    assert image.getpixel((20, 5))[:3] == (173, 156, 131)
    assert image.getpixel((20, 150))[:3] == (255, 255, 255)

    # Recovered 88x88 city board at x=89,y=32 with 17px cell pitch.
    assert image.getpixel((89, 32))[:3] == (255, 255, 255)
    assert image.getpixel((90, 33))[:3] == (64, 64, 64)
    assert image.getpixel((92, 35))[:3] == (120, 188, 40)
    assert image.getpixel((93, 36))[:3] == (67, 120, 23)

    # Browse-mode top-right status boxes start at width-51 after the 9px icon slot.
    assert image.getpixel((189, 1))[:3] == (199, 191, 178)
    assert image.getpixel((190, 2))[:3] == (173, 156, 131)
    assert image.getpixel((213, 1))[:3] == (199, 191, 178)



def test_city_background_exports_all_four_source_lot_themes() -> None:
    expected = (
        ((0x78, 0xBC, 0x28), (0x43, 0x78, 0x17)),
        ((0x7F, 0xAF, 0x46), (0x50, 0x6E, 0x37)),
        ((0x84, 0xA5, 0x5D), (0x58, 0x69, 0x50)),
        ((0x8A, 0x9C, 0x74), (0x62, 0x61, 0x6A)),
    )
    for theme, (outer, inner) in enumerate(expected):
        image = render_city_background(theme)
        assert image.getpixel((92, 35))[:3] == outer
        assert image.getpixel((93, 36))[:3] == inner
        assert image.getpixel((94, 37))[:3] == outer
        # Road/board geometry stays unchanged across themes.
        assert image.getpixel((90, 33))[:3] == (64, 64, 64)
        assert image.getpixel((120, 136))[:3] == (90, 142, 255)


def test_export_scene_backgrounds_writes_layered_construction_assets(
    tower_bloxx_jar: Path, tmp_path: Path
) -> None:
    graphics = tmp_path / "gba" / "graphics" / "backgrounds"
    graphics.mkdir(parents=True)
    for legacy in ("construction_bg_b1", "construction_bg_b2", "construction_bg_b3"):
        (graphics / f"{legacy}.bmp").write_bytes(b"legacy")
        (graphics / f"{legacy}.json").write_text("legacy")

    manifest = export_scene_backgrounds(tower_bloxx_jar, tmp_path)
    assert manifest["visible_size"] == [240, 160]
    assert manifest["construction_layer_size"] == [256, 512]
    assert manifest["resource43_entry_count"] == 88
    assert manifest["resource43_scaled_extent"] == 621
    assert manifest["scenery_chunk_centers"] == [0, 256, 512]

    sky_assets = [f"construction_sky_{index:02d}" for index in range(17)]
    scenery_assets = [f"construction_scenery_{index}" for index in range(3)]
    assert manifest["assets"] == [
        *sky_assets, *scenery_assets,
        "city_bg_theme_0", "city_bg_theme_1", "city_bg_theme_2", "city_bg_theme_3", "menu_bg",
    ]

    hashes = set()
    for name in sky_assets:
        bmp_path = graphics / f"{name}.bmp"
        bmp = Image.open(bmp_path)
        assert bmp.size == (256, 512)
        hashes.add(hashlib.sha256(bmp_path.read_bytes()).hexdigest())
        metadata = json.loads((graphics / f"{name}.json").read_text())
        assert metadata["type"] == "regular_bg"
    assert len(hashes) == 17

    for name in scenery_assets:
        bmp = Image.open(graphics / f"{name}.bmp")
        assert bmp.size == (256, 512)
        metadata = json.loads((graphics / f"{name}.json").read_text())
        assert metadata["type"] == "regular_bg"
        # Transparent scenery reserves palette index zero for empty pixels.
        assert 0 in set(bmp.get_flattened_data())

    for legacy in ("construction_bg_b1", "construction_bg_b2", "construction_bg_b3"):
        assert not (graphics / f"{legacy}.bmp").exists()
        assert not (graphics / f"{legacy}.json").exists()

    generated_header = tmp_path / "gba" / "include" / "generated" / "construction_background_data.h"
    header = generated_header.read_text(encoding="utf-8")
    assert "construction_background_decorations" in header
    assert header.count("ConstructionBackgroundDecoration{") == 12
    assert "construction_scenery_chunk_centers" in header

    ui_dir = tmp_path / "gba" / "graphics" / "ui"
    blink_bmp = Image.open(ui_dir / "construction_high_blink_p0.bmp")
    assert blink_bmp.size == (8, 8)
    assert len(set(blink_bmp.get_flattened_data())) == 2


def test_scene_background_export_removes_legacy_single_city_background(
    tower_bloxx_jar: Path, tmp_path: Path
) -> None:
    graphics = tmp_path / "gba" / "graphics" / "backgrounds"
    graphics.mkdir(parents=True)
    (graphics / "city_bg.bmp").write_bytes(b"legacy")
    (graphics / "city_bg.json").write_text("legacy")

    export_scene_backgrounds(tower_bloxx_jar, tmp_path)

    assert not (graphics / "city_bg.bmp").exists()
    assert not (graphics / "city_bg.json").exists()
