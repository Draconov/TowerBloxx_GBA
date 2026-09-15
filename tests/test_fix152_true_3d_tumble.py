from __future__ import annotations

from pathlib import Path

from tower_bloxx_extract.gba_project_export import TUMBLE_MESH_IDS, tumble_pose_angles

ROOT = Path(__file__).resolve().parents[1]


def test_five_degree_tumble_stage_contract() -> None:
    assert TUMBLE_MESH_IDS == (10, 11, 12, 13, 20, 21, 22, 23)
    assert tumble_pose_angles(0, False, False) == (0.0, 0.0)
    assert tumble_pose_angles(1, False, False) == (3.75, 5.0)
    assert tumble_pose_angles(12, False, False) == (45.0, 60.0)
    assert tumble_pose_angles(12, True, False) == (-45.0, 60.0)
    assert tumble_pose_angles(12, False, True) == (45.0, -60.0)
    assert tumble_pose_angles(12, True, True) == (-45.0, -60.0)


def test_exporter_emits_true_m3g_tumble_pose_lookup() -> None:
    source = (ROOT / "tools/tower_bloxx_extract/gba_project_export.py").read_text(encoding="utf-8")
    assert "struct TumblePoseAsset" in source
    assert "tumble_pose_for" in source
    assert "for stage in range(1, 13):" in source
    assert "post_rotate(draw_transform, y_angle, 0.0, 1.0, 0.0)" in source


def test_runtime_uses_prerendered_pose_when_y_tumble_is_nonzero() -> None:
    for relative in ("gba/src/quick_game_scene.cpp", "gba/src/tower_construction_scene.cpp"):
        source = (ROOT / relative).read_text(encoding="utf-8")
        header = (ROOT / relative.replace("src/", "include/tb/").replace(".cpp", ".h")).read_text(encoding="utf-8") if relative.endswith("quick_game_scene.cpp") else (ROOT / "gba/include/tb/tower_construction_scene.h").read_text(encoding="utf-8")
        assert "tumble_pose_for" in source
        assert "tumble_stage_for_y_angle" in source
        assert "_rendered_tumble_stage" in header
        assert "snapshot.current_y_angle_degrees, 0, _current_affine_mat" not in source
