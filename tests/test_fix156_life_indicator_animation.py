from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def test_both_gameplay_scenes_drive_house_life_animation_each_frame() -> None:
    for relative in ("gba/src/quick_game_scene.cpp", "gba/src/tower_construction_scene.cpp"):
        source = (ROOT / relative).read_text(encoding="utf-8")
        assert "_life_indicator_animation.reset(3);" in source
        assert "_life_indicator_animation.advance(delta_ms, snapshot.chances_left)" in source
        assert "life_indicator_changed ||" in source
        assert "_life_indicator_animation.frame_for_slot(slot, snapshot.chances_left" in source
