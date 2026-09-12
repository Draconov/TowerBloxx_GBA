from __future__ import annotations

import hashlib
import json
from pathlib import Path
import struct

from tower_bloxx_extract.gba_ui_export import EXTENDED_CHARACTERS, export_gba_ui_assets


EXPECTED_EXTENDED = (
    "¡", "°", "¿", "È", "à", "á", "â", "ä", "ç", "è", "é", "ê",
    "ì", "í", "ñ", "ò", "ó", "ô", "ö", "ù", "ú", "û", "ü",
)


def _tree_hash(root: Path) -> str:
    digest = hashlib.sha256()
    for path in sorted(p for p in root.rglob("*") if p.is_file()):
        digest.update(path.relative_to(root).as_posix().encode())
        digest.update(b"\0")
        digest.update(path.read_bytes())
        digest.update(b"\0")
    return digest.hexdigest()


def _bmp_geometry(path: Path) -> tuple[int, int, int]:
    data = path.read_bytes()
    assert data[:2] == b"BM"
    width, height = struct.unpack_from("<ii", data, 18)
    bpp = struct.unpack_from("<H", data, 28)[0]
    return width, height, bpp


def test_ui_export_is_complete_localized_and_deterministic(tower_bloxx_jar: Path, tmp_path: Path) -> None:
    a = tmp_path / "a"
    b = tmp_path / "b"
    manifest_a = export_gba_ui_assets(tower_bloxx_jar, a)
    manifest_b = export_gba_ui_assets(tower_bloxx_jar, b)

    assert manifest_a == manifest_b
    assert _tree_hash(a) == _tree_hash(b)
    assert manifest_a["locale_count"] == 5
    assert manifest_a["strings_per_locale"] == 134
    assert manifest_a["locale_codes"] == ["en-EN", "fr-FR", "it-IT", "de-DE", "es-ES"]
    assert tuple(manifest_a["utf8_characters"]) == EXPECTED_EXTENDED
    assert EXTENDED_CHARACTERS == EXPECTED_EXTENDED
    assert manifest_a["font_character_count"] == 95 + len(EXPECTED_EXTENDED)
    # Butano reserves space as a width-only character. Graphics start at '!'
    # and continue through '~', followed by the UTF-8 extension glyphs.
    assert manifest_a["font_graphics_count"] == 94 + len(EXPECTED_EXTENDED)
    assert manifest_a["source_resources"] == {
        "font_atlas": 36, "font_metrics": 44, "tower_logo": 7, "sumea_logo": 10,
        "menu_icons": [2, 3, 4, 5, 6], "menu_workers": [11, 12],
        "construction_target_badges": 13, "hud_white_digits": 14,
        "hud_brown_digits": 15, "hud_red_digits": 16, "hud_status_graphic": 17,
        "hud_state_indicators": 18, "quick_counter_frame": 19,
        "city_hanging_ui": 20, "city_status_icons": 21, "city_panels": 22,
        "city_action_icon": 23, "city_buildings": [24, 25, 26, 27], "city_lot": 28,
        "city_effects": 29, "crane_hook_frames": 30, "accuracy_stars": 35,
    }

    city_records = manifest_a["city_assets"]
    assert len(city_records["buildings"]) == 16
    assert len(city_records["lots"]) == 5
    assert {record["name"] for record in city_records["buildings"]} >= {"city_building_1_f0", "city_building_4_f3"}
    assert {record["name"] for record in city_records["lots"]} == {f"city_lot_f{index}" for index in range(5)}

    font_bmp = a / "gba/graphics/ui/tower_font.bmp"
    selected_font_bmp = a / "gba/graphics/ui/tower_font_selected.bmp"
    assert _bmp_geometry(font_bmp) == ((94 + len(EXPECTED_EXTENDED)) * 8, 16, 4)
    assert _bmp_geometry(selected_font_bmp) == ((94 + len(EXPECTED_EXTENDED)) * 8, 16, 4)
    font_meta = json.loads((a / "gba/graphics/ui/tower_font.json").read_text())
    selected_font_meta = json.loads((a / "gba/graphics/ui/tower_font_selected.json").read_text())
    expected_font_meta = {"bpp_mode": "bpp_4", "height": 16, "type": "sprite", "width": 8}
    assert font_meta == expected_font_meta
    assert selected_font_meta == expected_font_meta

    localization = (a / "gba/include/generated/tower_localization.h").read_text(encoding="utf-8")
    assert "Tower Bloxx(TM)" in localization
    assert "Français" in localization
    assert "Español" in localization
    assert "inline constexpr int strings_per_locale = 134;" in localization
    assert "quick_game_instruction_lines" in localization
    assert "build_city_instruction_lines" in localization
    assert "about_lines" in localization
    assert "instruction_lines_per_page = 8" in localization

    font_header = (a / "gba/include/generated/tower_font.h").read_text(encoding="utf-8")
    assert "bn::sprite_items::tower_font" in font_header
    assert "bn::sprite_items::tower_font_selected" in font_header
    assert "selected_tower_font" in font_header
    assert "space_between_characters" in font_header
    assert "tower_font_character_widths,\n        space_between_characters);" in font_header

    assert manifest_a["adapted_instructions"]["en-EN"]["quick"].find("Press A") >= 0
    assert "Press 5" not in manifest_a["adapted_instructions"]["en-EN"]["quick"]
    assert "D-pad" in manifest_a["adapted_instructions"]["en-EN"]["city"]
    assert "20 milestones" in manifest_a["adapted_instructions"]["en-EN"]["city"]

    logo_files = [record["path"] for record in manifest_a["files"] if "tower_bloxx_logo" in record["path"]]
    sumea_files = [record["path"] for record in manifest_a["files"] if "sumea_logo" in record["path"]]
    assert logo_files
    assert sumea_files
    assert manifest_a["procedural_assets"] == ["menu_highlight"]
    assert {record["name"] for record in manifest_a["menu_assets"]["icons"]} == {
        "menu_continue_icon", "menu_build_city_icon", "menu_quick_game_icon",
        "menu_settings_icon", "menu_exit_icon",
    }
    assert len(manifest_a["menu_assets"]["worker_blue_frames"]) == 10
    assert len(manifest_a["menu_assets"]["worker_red_frames"]) == 10
    assert {record["name"] for record in manifest_a["menu_assets"]["worker_blue_frames"]} == {
        f"menu_worker_blue_f{index}" for index in range(10)
    }
    assert {record["name"] for record in manifest_a["menu_assets"]["worker_red_frames"]} == {
        f"menu_worker_red_f{index}" for index in range(10)
    }

    hud = manifest_a["hud_assets"]
    assert len(hud["construction_target_badges"]) == 5
    assert len(hud["white_digits"]) == 14
    assert len(hud["brown_digits"]) == 12
    assert len(hud["red_digits"]) == 11
    assert hud["status_graphic"]["name"] == "hud_status_graphic"
    assert hud["population_icon"]["name"] == "hud_population_icon"
    assert len(hud["state_indicators"]) == 10
    assert hud["quick_counter_frame"]["name"] == "quick_counter_frame"
    assert len(hud["crane_hook_frames"]) == 5
    assert len(hud["accuracy_stars"]) == 3

    city_supplemental = manifest_a["city_assets"]["supplemental"]
    assert city_supplemental["hanging_ui"]["name"] == "city_hanging_ui"
    assert len(city_supplemental["status_icons"]) == 5
    assert len(city_supplemental["panels"]) == 2
    assert city_supplemental["action_icon"]["name"] == "city_action_icon"
    assert len(city_supplemental["effects"]) == 6
    highlight = manifest_a["menu_highlight"]
    assert highlight["name"] == "menu_highlight"
    assert highlight["width"] == 230
    assert highlight["height"] == 16
    assert highlight["parts"]
    ui_assets_header = (a / "gba/include/generated/tower_ui_assets.h").read_text(encoding="utf-8")
    assert "menu_highlight_parts" in ui_assets_header
    assert "menu_build_city_icon_parts" in ui_assets_header
    assert "menu_worker_blue_f0_parts" in ui_assets_header
    assert "menu_worker_red_f0_parts" in ui_assets_header
    assert "quick_counter_frame_parts" in ui_assets_header
    assert "construction_target_badge_f0_parts" in ui_assets_header
    assert "hud_white_digit_f0_parts" in ui_assets_header
    assert "hud_state_indicator_f0_parts" in ui_assets_header

    # Default k.b(Graphics) palette branch used by the captured JAR: yellow
    # selection band (0xFFDD46) and dark-red selected glyphs (0xA50003).
    assert _bmp_palette_entry(a / "gba/graphics/ui/menu_highlight_p0.bmp", 1)[:3] == (248, 216, 64)
    assert _bmp_palette_entry(selected_font_bmp, 1)[:3] == (160, 0, 0)


def _bmp_palette_entry(path: Path, index: int) -> tuple[int, int, int, int]:
    data = path.read_bytes()
    offset = 14 + 40 + index * 4
    blue, green, red, reserved = data[offset:offset + 4]
    return red, green, blue, reserved


def test_ui_export_keeps_opaque_near_black_distinct_from_transparency(tower_bloxx_jar: Path, tmp_path: Path) -> None:
    project = tmp_path / "project"
    export_gba_ui_assets(tower_bloxx_jar, project)

    font_bmp = project / "gba/graphics/ui/tower_font.bmp"
    transparent = _bmp_palette_entry(font_bmp, 0)
    opaque_dark = _bmp_palette_entry(font_bmp, 1)

    assert transparent == (0, 0, 0, 0)
    assert opaque_dark != transparent
    assert opaque_dark[:3] == (8, 8, 8)
