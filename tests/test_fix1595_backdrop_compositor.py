from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "gba/src/construction_backdrop.cpp"


def _source() -> str:
    return SOURCE.read_text(encoding="utf-8")


def test_sky_and_scenery_have_explicit_stable_z_order() -> None:
    source = _source()
    assert "constexpr int construction_sky_bg_z_order = 1;" in source
    assert "constexpr int construction_scenery_bg_z_order = 0;" in source
    assert "_sky_background->set_priority(3);" in source
    assert "_sky_background->set_z_order(construction_sky_bg_z_order);" in source
    assert "_scenery_background->set_priority(3);" in source
    assert "_scenery_background->set_z_order(construction_scenery_bg_z_order);" in source


def test_high_altitude_world_sprites_use_gameplay_bg_priority() -> None:
    source = _source()
    assert "sprite.set_bg_priority(3);" in source
    assert "blink.set_bg_priority(3);" in source
    assert "set_bg_priority(2);" not in source


def test_world_sprite_z_order_stays_behind_modal_ui() -> None:
    source = _source()
    assert "sprite.set_z_order(100);" in source
    assert "blink.set_z_order(100);" in source

    tower = (ROOT / "gba/src/tower_construction_scene.cpp").read_text(encoding="utf-8")
    assert "constexpr int modal_backdrop_z_order = -90;" in tower
    assert "_text_generator.set_z_order(-100);" in tower
