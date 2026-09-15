from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def test_result_tracks_replace_tower_loop_instead_of_jingle_overlay() -> None:
    source = (ROOT / "gba/src/game_audio.cpp").read_text(encoding="utf-8")
    header = (ROOT / "gba/include/tb/game_audio.h").read_text(encoding="utf-8")

    assert '#include "bn_jingle.h"' not in source
    assert "play_jingle" not in source
    assert "bn::music::stop();" in source
    for name in ("construction_fail", "normal_roof", "trophy_roof"):
        assert f"bn::music_items::{name}.play(0.5, false);" in source
    assert "bool _result_active = false;" in header


def test_scene_loop_resumes_only_after_one_shot_finishes() -> None:
    source = (ROOT / "gba/src/game_audio.cpp").read_text(encoding="utf-8")
    assert "if(_result_active)" in source
    assert "if(bn::music::playing())" in source
    assert "_result_active = false;" in source
    assert "_play_scene(_scene);" in source


def test_disabling_sound_cancels_one_shot_and_loop_state() -> None:
    source = (ROOT / "gba/src/game_audio.cpp").read_text(encoding="utf-8")
    assert "_result_active = false;" in source
    assert "_started = false;" in source
