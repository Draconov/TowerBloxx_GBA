from __future__ import annotations

from pathlib import Path

from PIL import Image

from tower_bloxx_extract.gba_ui_export import export_gba_ui_assets

ROOT = Path(__file__).resolve().parents[1]


def test_combo_meter_matches_exact_240x160_jar_geometry(
    tower_bloxx_jar: Path, tmp_path: Path
) -> None:
    project = tmp_path / "project"
    manifest = export_gba_ui_assets(tower_bloxx_jar, project)
    frame = manifest["hud_assets"]["quick_combo_meter_frame"]
    fill = manifest["hud_assets"]["quick_combo_meter_fill"]
    flash = manifest["hud_assets"]["quick_combo_meter_flash"]

    # House.i(Graphics) on the 240x160 specialization:
    # outer drawRect: (57,9), 125x8 => 126x9 pixel bounds
    # inner drawRect: (58,10), 123x6
    # live fillRect:  (60,12), width=Y*120/6000, height=3
    assert (frame["width"], frame["height"]) == (126, 9)
    assert (fill["width"], fill["height"]) == (64, 3)
    assert (flash["width"], flash["height"]) == (64, 3)

    graphics = project / "gba/graphics/ui"
    frame_bmps = [Image.open(graphics / f"quick_combo_meter_frame_p{i}.bmp") for i in range(len(frame["parts"]))]
    # The exact two frame colors must both survive export: dark-brown outer
    # border and yellow inner border.
    seen_rgb: set[tuple[int, int, int]] = set()
    for image in frame_bmps:
        palette = image.getpalette()
        assert palette is not None
        for index in set(image.get_flattened_data()):
            if index:
                seen_rgb.add(tuple(palette[index * 3 : index * 3 + 3]))
    assert (104, 24, 0) in seen_rgb or (107, 24, 0) in seen_rgb or (104, 24, 0) in seen_rgb
    assert any(r >= 248 and g >= 248 and b <= 8 for r, g, b in seen_rgb)

    source = (ROOT / "gba/src/quick_game_scene.cpp").read_text(encoding="utf-8")
    assert "constexpr int combo_meter_fill_left = -60;" in source
    assert "constexpr int combo_meter_fill_y = -67;" in source
    assert "constexpr int combo_meter_frame_x = 0;" in source
    assert "constexpr int combo_meter_frame_y = -67;" in source
    assert "constexpr int combo_readout_x = 68;" in source
    assert "constexpr int combo_readout_y = -65;" in source
    assert "show_ui_composite(*hud_brown_digit_frames[11], combo_readout_x, combo_readout_y" in source


def test_bulldozer_slot_is_lower_centered_and_uses_replacement_destruction_effect() -> None:
    source = (ROOT / "gba/src/build_city_scene.cpp").read_text(encoding="utf-8")
    start = source.index("if(snapshot.mode == BuildCityMode::Placement)")
    end = source.index("const int cell_center_x", start)
    placement = source[start:end]

    # User-requested GBA presentation adjustment: move the 21x21 bulldozer
    # action cell down by 10 px from the recovered (72,107) centre.
    assert "constexpr int discard_cell_center_x = 72;" in source
    assert "constexpr int discard_cell_center_y = 117;" in source
    compact = " ".join(placement.split())
    assert "generated::city_action_icon, centered_x(discard_cell_center_x), centered_y(discard_cell_center_y)" in compact

    discard = placement[placement.index("if(snapshot.cursor_column < 0)") :]
    compact_discard = " ".join(discard.split())
    # Pending building is centered in the bulldozer cell, not baseline-aligned.
    assert "centered_x(discard_cell_center_x)" in compact_discard
    assert "centered_y(discard_cell_center_y)" in compact_discard
    # During commit the discard path uses the exact replacement/destruction
    # frame sequence from resource 29 and draws it at the same cell centre.
    assert "build_city_discard_effect_frame(snapshot.placement_timer_ms)" in compact_discard
    assert "*city_effects[effect_frame]" in compact_discard
    assert "centered_x(discard_cell_center_x)" in compact_discard
    assert "centered_y(discard_cell_center_y)" in compact_discard
