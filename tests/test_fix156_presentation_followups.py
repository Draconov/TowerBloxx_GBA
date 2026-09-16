from __future__ import annotations

from pathlib import Path

from PIL import Image

from tower_bloxx_extract.scene_background_export import export_scene_backgrounds

ROOT = Path(__file__).resolve().parents[1]


def test_combo_meter_uses_exact_240x160_jar_anchor() -> None:
    source = (ROOT / "gba/src/quick_game_scene.cpp").read_text(encoding="utf-8")
    assert "constexpr int combo_meter_fill_left = -60;" in source
    assert "constexpr int combo_meter_frame_x = 0;" in source
    assert "constexpr int combo_meter_frame_y = -67;" in source
    assert "constexpr int combo_meter_fill_y = -67;" in source
    assert "generated::quick_combo_meter_frame, combo_meter_frame_x, combo_meter_frame_y" in source
    assert "show_ui_composite(*hud_brown_digit_frames[11], 66, -67" in source


def test_city_background_keeps_source_white_message_panel_opaque(
    tower_bloxx_jar: Path, tmp_path: Path
) -> None:
    export_scene_backgrounds(tower_bloxx_jar, tmp_path)
    bmp = Image.open(tmp_path / "gba/graphics/backgrounds/city_bg_theme_0.bmp")

    # The 240x160 visible image is centered in the 256x256 regular BG.  The
    # JAR's bottom panel begins at visible y=137 and is source-white from y=139.
    panel_x = 8 + 120
    panel_y = 48 + 150
    index = bmp.getpixel((panel_x, panel_y))
    palette = bmp.getpalette()
    assert palette is not None
    rgb = tuple(palette[index * 3 : index * 3 + 3])

    # Regular BG palette index 0 is transparent in Butano; source white must
    # therefore live in a non-zero slot or it shows the blue backdrop instead.
    assert index != 0
    assert rgb == (248, 248, 248)


def test_build_city_bulldozer_uses_adjusted_position_and_pending_tower_remains_visible() -> None:
    source = (ROOT / "gba/src/build_city_scene.cpp").read_text(encoding="utf-8")
    start = source.index("if(snapshot.mode == BuildCityMode::Placement)")
    end = source.index("const int cell_center_x", start)
    placement = source[start:end]

    assert "constexpr int discard_cell_center_x = 72;" in source
    assert "constexpr int discard_cell_center_y = 117;" in source
    compact = " ".join(placement.split())
    assert "generated::city_action_icon, centered_x(discard_cell_center_x), centered_y(discard_cell_center_y)" in compact
    assert "if(snapshot.cursor_column < 0)" in placement
    discard = placement[placement.index("if(snapshot.cursor_column < 0)") :]
    assert "building_asset(type, snapshot.pending_roof)" in discard
    assert "return;" in discard
    assert discard.index("building_asset(type, snapshot.pending_roof)") < discard.index("return;")


def test_quick_results_remain_visible_until_acknowledged_before_score_flow() -> None:
    source = (ROOT / "gba/src/quick_game_scene.cpp").read_text(encoding="utf-8")
    start = source.index("QuickGameSceneUpdateResult QuickGameScene::update")
    end = source.index("void QuickGameScene::_rebuild_floor_sprites", start)
    body = source[start:end]

    result_ack = body[body.index("if(before_status == QuickGameStatus::Results") : body.index("// 16,17,17 ms", body.index("if(before_status == QuickGameStatus::Results"))]
    assert "input.pressed(Key::A)" in result_ack
    assert "result.score_ready = true;" in result_ack
    assert "result.final_population" in result_ack

    entered_results = body[body.index("if(snapshot.status == QuickGameStatus::Results && ! _records_applied)") :]
    records_block = entered_results[: entered_results.index("if(snapshot.floor_count", 1)]
    assert "apply_quick_result" in records_block
    assert "result.score_ready = true;" not in records_block

    hud = source[source.index("if(snapshot.status == QuickGameStatus::Results)") : source.index("// Exact lower-left Quick Game frame")]
    assert "generated::dialog_window" in hud
    assert "format_result_line" in hud
