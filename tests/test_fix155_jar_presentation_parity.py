from __future__ import annotations

from pathlib import Path

from PIL import Image

from tower_bloxx_extract.gba_ui_export import export_gba_ui_assets

ROOT = Path(__file__).resolve().parents[1]


def _palette16(path: Path) -> tuple[int, ...]:
    image = Image.open(path)
    palette = image.getpalette()
    assert palette is not None
    return tuple(palette[: 16 * 3])


def test_build_city_placement_keeps_demolition_action_visible() -> None:
    source = (ROOT / "gba/src/build_city_scene.cpp").read_text(encoding="utf-8")
    marker = "if(snapshot.mode == BuildCityMode::Placement)"
    assert marker in source
    placement = source[source.index(marker): source.index("const int cell_center_x", source.index(marker))]
    assert "generated::city_action_icon" in placement
    assert placement.index("generated::city_action_icon") < placement.index("if(snapshot.cursor_column < 0)")


def test_quick_combo_meter_assets_are_generated_and_share_hud_palette(
    tower_bloxx_jar: Path, tmp_path: Path
) -> None:
    project = tmp_path / "project"
    manifest = export_gba_ui_assets(tower_bloxx_jar, project)
    ui = project / "gba/graphics/ui"

    procedural = set(manifest["procedural_assets"])
    assert "quick_combo_meter_frame" in procedural
    assert "quick_combo_meter_fill" in procedural
    assert "quick_combo_meter_flash" in procedural

    shared = _palette16(ui / "hud_brown_digit_f11_p0.bmp")
    for name in (
        "quick_combo_meter_frame_p0.bmp",
        "quick_combo_meter_fill_p0.bmp",
        "quick_combo_meter_flash_p0.bmp",
    ):
        assert _palette16(ui / name) == shared


def test_quick_combo_meter_uses_source_120px_timer_geometry() -> None:
    source = (ROOT / "gba/src/quick_game_scene.cpp").read_text(encoding="utf-8")
    assert "constexpr int combo_meter_max_width = 120;" in source
    assert "snapshot.combo_meter_ms * combo_meter_max_width / 6000" in source
    assert "snapshot.combo_meter_ms > 5850" in source
    assert "generated::quick_combo_meter_frame" in source
    assert "generated::quick_combo_meter_fill" in source
    assert "generated::quick_combo_meter_flash" in source


def test_quick_combo_meter_fill_applies_generated_composite_y_offset() -> None:
    """The 4px fill sits at the top of a 32px OBJ canvas and needs its +14px part offset."""
    source = (ROOT / "gba/src/quick_game_scene.cpp").read_text(encoding="utf-8")
    start = source.index("void QuickGameScene::_update_combo_meter")
    end = source.index("void QuickGameScene::_rebuild_hud", start)
    body = source[start:end]
    assert "const int part_y = flash ? generated::quick_combo_meter_flash.parts[0].y" in body
    assert "generated::quick_combo_meter_fill.parts[0].y;" in body
    assert "combo_meter_fill_y + part_y" in body


def test_quick_results_use_dialog_window_and_gba_a_prompt() -> None:
    source = (ROOT / "gba/src/quick_game_scene.cpp").read_text(encoding="utf-8")
    start = source.index("if(snapshot.status == QuickGameStatus::Results)")
    end = source.index("// Exact lower-left Quick Game frame", start)
    body = source[start:end]
    assert "generated::dialog_window" in body
    assert 'bn::string<32> back_text("A  ")' in body
    assert 'bn::string<32> back_text("A/B  ")' not in body


def test_gameplay_workers_render_in_front_of_tower_meshes() -> None:
    source = (ROOT / "gba/src/quick_game_scene.cpp").read_text(encoding="utf-8")
    assert "constexpr int gameplay_worker_z_order = -30;" in source
    start = source.index("void QuickGameScene::_rebuild_worker_sprites")
    end = source.index("void QuickGameScene::_rebuild_hud", start)
    body = source[start:end]
    assert "gameplay_worker_z_order" in body
