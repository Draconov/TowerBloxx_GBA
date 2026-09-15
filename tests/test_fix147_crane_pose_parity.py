from __future__ import annotations

import json
from pathlib import Path

from tower_bloxx_extract.gba_project_export import export_gba_project_assets


def test_normal_hook_exports_all_recovered_rotation_steps(tower_bloxx_jar: Path, tmp_path: Path) -> None:
    project = tmp_path / "project"
    manifest = export_gba_project_assets(tower_bloxx_jar, project)

    frames = manifest["crane_hook_frames"]
    assert [frame["rotation_step"] for frame in frames] == list(range(-24, 25))
    assert len(frames) == 49

    graphics = project / "gba" / "graphics" / "gameplay"
    for frame in frames:
        assert frame["parts"]
        for part in frame["parts"]:
            asset = part["asset"]
            assert (graphics / f"{asset}.bmp").is_file()
            metadata = json.loads((graphics / f"{asset}.json").read_text(encoding="utf-8"))
            assert metadata["bpp_mode"] == "bpp_4"

    header = (project / "gba" / "include" / "generated" / "tower_mesh_assets.h").read_text(encoding="utf-8")
    assert "crane_hook_frame_count = 49" in header
    assert "crane_hook_frame_for_step" in header


def test_runtime_uses_prerendered_hook_and_exact_special_cable_projection() -> None:
    root = Path(__file__).resolve().parents[1]
    for relative in ("gba/src/quick_game_scene.cpp", "gba/src/tower_construction_scene.cpp"):
        source = (root / relative).read_text(encoding="utf-8")
        assert "crane_hook_frame_for_step(snapshot.crane_x >> 4)" in source
        assert "position_rotated_mesh_part(\n                        crane_mesh.parts[part_index]" not in source
        assert "const int end_x = _screen_x(snapshot.crane_x);" in source
        assert "const int end_y = _screen_y(snapshot.crane_y + 528, snapshot.presentation_camera_y);" in source


def test_build_city_highlighted_unlocked_tower_stays_raised() -> None:
    root = Path(__file__).resolve().parents[1]
    source = (root / "gba/src/build_city_scene.cpp").read_text(encoding="utf-8")
    # Approved GBA UX adaptation: the highlighted unlocked tower and its red
    # outline stay +2px right / -2px up for the full browse selection, not only
    # during the source confirmation timer. The orange pulse itself never moves it.
    assert "build_city_preview_raised(selected, snapshot.selected_building_type," in source
    assert "if(type == selected)" in source
    assert "snapshot.construction_select_ms > 0 && snapshot.construction_select_ms < 250" not in source
    assert "_selector_flash_ms < 250" not in source
