from pathlib import Path


def test_tower_construction_switches_between_house_mesh8_and_mesh7_and_draws_special_cable() -> None:
    root = Path(__file__).resolve().parents[1]
    source = (root / "gba/src/tower_construction_scene.cpp").read_text()
    header = (root / "gba/include/tb/tower_construction_scene.h").read_text()
    assert "constexpr int special_crane_mesh_id = 7;" in source
    assert "snapshot.roof_phase ? special_crane_mesh_id : crane_hook_mesh_id" in source
    assert "_rendered_crane_mesh_id" in header
    assert "crane_special_cable_segment" in source
    assert "_rebuild_special_cable(snapshot)" in source


def test_quick_and_construction_current_block_apply_recovered_y_axis_perspective() -> None:
    root = Path(__file__).resolve().parents[1]
    quick = (root / "gba/src/quick_game_scene.cpp").read_text()
    construction = (root / "gba/src/tower_construction_scene.cpp").read_text()
    for source in (quick, construction):
        assert "current_y_angle_degrees" in source
        assert "set_horizontal_scale" in source
        assert "y_angle_degrees" in source
