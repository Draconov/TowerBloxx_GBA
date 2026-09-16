from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def test_opaque_construction_backgrounds_share_sprite_safe_priority() -> None:
    source = (ROOT / 'gba/src/construction_backdrop.cpp').read_text(encoding='utf-8')
    assert '_sky_background->set_priority(3);' in source
    assert '_scenery_background->set_priority(3);' in source
    assert '_scenery_background->set_priority(2);' not in source
