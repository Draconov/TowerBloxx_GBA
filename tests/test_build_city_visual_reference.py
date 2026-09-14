from __future__ import annotations

from pathlib import Path
import subprocess

from PIL import Image

from tower_bloxx_extract.gba_ui_export import export_gba_ui_assets


def test_build_city_valid_lot_color_math_matches_m_class(tmp_path: Path) -> None:
    root = Path(__file__).resolve().parents[1]
    header = root / "gba/include/tb/build_city_visuals.h"
    source = root / "gba/src/build_city_visuals.cpp"
    assert header.is_file(), "Fix 14 visual helper header is missing"
    assert source.is_file(), "Fix 14 visual helper source is missing"

    output = tmp_path / "build_city_visuals_test"
    command = [
        "g++", "-std=c++20", "-Wall", "-Wextra", "-Werror", "-pedantic",
        "-I", str(root / "gba/include"),
        str(source), str(root / "tests/cpp/build_city_visuals_test.cpp"),
        "-o", str(output),
    ]
    built = subprocess.run(command, cwd=root, capture_output=True, text=True)
    assert built.returncode == 0, built.stdout + built.stderr
    ran = subprocess.run([str(output)], cwd=root, capture_output=True, text=True)
    assert ran.returncode == 0, ran.stdout + ran.stderr
    assert ran.stdout == "build city visuals ok\n"


def _opaque_colors(path: Path) -> set[tuple[int, int, int]]:
    image = Image.open(path).convert("RGB")
    return {color for _count, color in image.getcolors(maxcolors=65536) or [] if color != (0, 0, 0)}


def test_fix14_export_has_exact_city_visual_primitives(tower_bloxx_jar: Path, tmp_path: Path) -> None:
    project = tmp_path / "project"
    manifest = export_gba_ui_assets(tower_bloxx_jar, project)

    assert manifest["procedural_assets"] == [
        "menu_highlight", "city_progress_segment", "city_valid_lot_ring",
        "city_comparison_panel_active", "city_type_badge_1", "city_type_badge_2",
        "city_type_badge_3", "city_type_badge_4",
    ]

    supplemental = manifest["city_assets"]["supplemental"]
    assert supplemental["valid_lot_ring"]["width"] == 14
    assert supplemental["valid_lot_ring"]["height"] == 14
    assert supplemental["comparison_panel_active"]["width"] == 24
    assert supplemental["comparison_panel_active"]["height"] == 9
    assert [(record["width"], record["height"]) for record in supplemental["type_badges"]] == [(4, 7)] * 4

    # Resource 21 source x=9..15 is the orange placement glyph and x=16..22
    # is the neutral gray browse glyph.  This catches the Fix-11 name swap.
    graphics = project / "gba/graphics/ui"
    placement_colors = _opaque_colors(graphics / "city_status_placement_p0.bmp")
    browse_colors = _opaque_colors(graphics / "city_status_browse_p0.bmp")
    assert any(red >= 200 and green >= 120 and blue < 80 for red, green, blue in placement_colors)
    assert all(abs(red - green) <= 48 and blue <= green + 24 for red, green, blue in browse_colors)

    # Procedural panel/badge colors are source Java2D constants quantized by
    # the exporter to GBA-compatible palette steps.
    panel_colors = _opaque_colors(graphics / "city_comparison_panel_active_p0.bmp")
    assert (224, 224, 224) in panel_colors
    badge1 = _opaque_colors(graphics / "city_type_badge_1_p0.bmp")
    assert (64, 112, 208) in badge1 or (64, 112, 216) in badge1


def test_fix14_build_city_scene_uses_recovered_hud_anchors_and_continuous_ring() -> None:
    root = Path(__file__).resolve().parents[1]
    header = (root / "gba/include/tb/build_city_scene.h").read_text()
    scene = (root / "gba/src/build_city_scene.cpp").read_text()

    assert "_show_status(const SaveData& save, const BuildCitySnapshot& snapshot)" in header
    assert "tb/build_city_visuals.h" in scene
    assert "generated::city_valid_lot_ring" in scene
    assert "_valid_lot_palette" not in header
    assert "create_new_palette()" not in scene
    assert "bn::optional<bn::sprite_palette_ptr> valid_lot_palette;" in scene
    assert "valid_lot_palette = sprite.palette()" in scene
    assert "generated::city_comparison_panel_active" in scene
    assert "generated::city_type_badge_1" in scene
    assert "centered_y(11)" in scene  # resource-20 top clips
    assert "centered_x(23 + cell * 8), centered_y(5)" in scene  # resource-22 population backing
    assert "centered_x(201), centered_y(5)" in scene
    assert "centered_x(225), centered_y(5)" in scene
    assert "211" in scene and "235" in scene  # exact white-digit right anchors
    assert "* 4" in scene  # exact white-digit spacing
    assert "_placement_flash_ms >= 400" not in scene


def test_valid_lot_palette_does_not_consume_a_second_obj_palette_bank(
    tower_bloxx_jar: Path, tmp_path: Path
) -> None:
    root = Path(__file__).resolve().parents[1]
    header = (root / "gba/include/tb/build_city_scene.h").read_text()
    scene = (root / "gba/src/build_city_scene.cpp").read_text()

    # Fix-14 originally allocated a fresh 16-color OBJ palette and then
    # created each ring sprite with its source palette before swapping it.
    # On real GBA/Butano this can transiently require a second palette bank
    # and panic once all 16 OBJ BPP4 banks are occupied.
    assert "create_new_palette()" not in scene
    assert "_valid_lot_palette" not in header
    assert "valid_lot_palette = sprite.palette()" in scene
    assert "valid_lot_palette->set_color(1, color)" in scene

    project = tmp_path / "project"
    export_gba_ui_assets(tower_bloxx_jar, project)
    graphics = project / "gba/graphics/ui"
    ring = Image.open(graphics / "city_valid_lot_ring_p0.bmp")
    arrow = Image.open(graphics / "city_continue_arrow_p0.bmp")
    # The mutable ring starts from a sacrificial palette that is not shared
    # with the white continue arrow or another UI item before recoloring.
    assert ring.getpalette()[:48] != arrow.getpalette()[:48]
