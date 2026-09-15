from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def test_build_city_scene_uses_four_theme_backgrounds_and_population_roll() -> None:
    source = (ROOT / "gba/src/build_city_scene.cpp").read_text(encoding="utf-8")
    header = (ROOT / "gba/include/tb/build_city_scene.h").read_text(encoding="utf-8")

    for theme in range(4):
        assert f'bn_regular_bg_items_city_bg_theme_{theme}.h' in source
        assert f'city_bg_theme_{theme}.create_bg' in source

    assert "BuildCityPopulationRoll _population_roll" in header
    assert "int _display_population" in header
    assert "_population_roll.start(_display_population, snapshot.total_population);" in source
    assert "_population_roll.update(delta_ms);" in source
    assert "_population_roll.panel_state_for_cell(cell)" in source
    assert "_population_roll.use_red_digits()" in source
    assert "hud_red_digit_f0" in source


def test_build_city_scene_rebuilds_while_population_roll_is_active() -> None:
    source = (ROOT / "gba/src/build_city_scene.cpp").read_text(encoding="utf-8")
    assert "const bool population_animation = _population_roll.animating();" in source
    assert "browse_animation || placement_animation || population_animation" in source


def test_population_roll_hides_changed_suffix_digits_while_cells_roll() -> None:
    source = (ROOT / "gba/src/build_city_scene.cpp").read_text(encoding="utf-8")
    assert "const int visible_population_digits = 5 - _population_roll.changed_cells();" in source
    assert "index < visible_population_digits" in source
