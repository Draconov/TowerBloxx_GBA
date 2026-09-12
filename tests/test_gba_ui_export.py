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
    assert manifest_a["source_resources"] == {"font_atlas": 36, "font_metrics": 44, "tower_logo": 7, "sumea_logo": 10, "city_buildings": [24, 25, 26, 27], "city_lot": 28}

    city_records = manifest_a["city_assets"]
    assert len(city_records["buildings"]) == 16
    assert len(city_records["lots"]) == 5
    assert {record["name"] for record in city_records["buildings"]} >= {"city_building_1_f0", "city_building_4_f3"}
    assert {record["name"] for record in city_records["lots"]} == {f"city_lot_f{index}" for index in range(5)}

    font_bmp = a / "gba/graphics/ui/tower_font.bmp"
    assert _bmp_geometry(font_bmp) == ((95 + len(EXPECTED_EXTENDED)) * 8, 16, 4)
    font_meta = json.loads((a / "gba/graphics/ui/tower_font.json").read_text())
    assert font_meta == {"bpp_mode": "bpp_4", "height": 16, "type": "sprite", "width": 8}

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
    assert "space_between_characters" in font_header

    assert manifest_a["adapted_instructions"]["en-EN"]["quick"].find("Press A") >= 0
    assert "Press 5" not in manifest_a["adapted_instructions"]["en-EN"]["quick"]
    assert "D-pad" in manifest_a["adapted_instructions"]["en-EN"]["city"]
    assert "20 milestones" in manifest_a["adapted_instructions"]["en-EN"]["city"]

    logo_files = [record["path"] for record in manifest_a["files"] if "tower_bloxx_logo" in record["path"]]
    sumea_files = [record["path"] for record in manifest_a["files"] if "sumea_logo" in record["path"]]
    assert logo_files
    assert sumea_files


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
