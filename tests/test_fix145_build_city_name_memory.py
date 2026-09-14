from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def test_main_only_opens_ui_when_build_city_submission_requires_it() -> None:
    text = (ROOT / "gba/src/main.cpp").read_text()
    block = text.split("else if(result.placement_committed)", 1)[1].split("else if(result.exit)", 1)[0]
    assert "ScoreSubmissionBeginResult" in block or "auto score_submission" in block
    assert "score_submission.requires_ui" in block
    assert "score_submission.save_dirty" in block
    assert block.index("score_submission.requires_ui") < block.index("session.show_ui()")


def test_name_memory_reuses_existing_save_bytes() -> None:
    header = (ROOT / "gba/include/tb/save_data.h").read_text()
    assert "inline constexpr uint16_t save_version = 4;" in header
    hall = (ROOT / "gba/include/tb/hall_of_fame.h").read_text()
    assert "std::array<uint8_t, 3> reserved{}" in hall
