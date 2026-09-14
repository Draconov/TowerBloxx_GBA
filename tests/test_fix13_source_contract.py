from pathlib import Path


def test_quick_and_tower_construction_follow_recovered_house_crane_state_machine() -> None:
    root = Path(__file__).resolve().parents[1]
    quick = (root / "gba/src/quick_game_scene.cpp").read_text()
    construction = (root / "gba/src/tower_construction_scene.cpp").read_text()
    tower_header = (root / "gba/include/tb/tower_construction_scene.h").read_text()
    quick_header = (root / "gba/include/tb/quick_game_scene.h").read_text()

    for source in (quick, construction):
        assert "constexpr int special_crane_mesh_id = 7;" in source
        assert "crane_presentation_mode(" in source
        assert "CranePresentationMode::Special" in source
        assert "CranePresentationMode::Hidden" in source
        assert "crane_special_cable_segment" in source

    assert "_rebuild_special_cable(snapshot, crane_mode)" in construction
    assert "_rebuild_special_cable(snapshot, mode)" in quick
    assert "_rendered_crane_mesh_id" in tower_header
    assert "_rendered_crane_mesh_id" in quick_header


def test_quick_and_construction_current_block_apply_recovered_y_axis_perspective() -> None:
    root = Path(__file__).resolve().parents[1]
    quick = (root / "gba/src/quick_game_scene.cpp").read_text()
    construction = (root / "gba/src/tower_construction_scene.cpp").read_text()
    for source in (quick, construction):
        assert "current_y_angle_degrees" in source
        assert "set_horizontal_scale" in source
        assert "y_angle_degrees" in source
