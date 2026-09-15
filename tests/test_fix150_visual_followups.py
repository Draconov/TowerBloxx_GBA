from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def test_empty_lot_commit_draws_new_building_under_smoke_but_replacements_wait() -> None:
    source = read("gba/src/build_city_scene.cpp")

    assert "const bool replacing_existing_tile = target_is_grid_cell &&" in source
    assert "save.city_tiles[snapshot.cursor_row * 5 + snapshot.cursor_column].type != 0;" in source
    assert "const bool show_pending_building = snapshot.pending_building_type >= 1 &&" in source
    assert "(! snapshot.placement_committing || ! replacing_existing_tile);" in source


def test_modal_backdrop_is_drawn_for_build_city_and_construction_messages() -> None:
    build_city_header = read("gba/include/tb/build_city_scene.h")
    build_city_source = read("gba/src/build_city_scene.cpp")
    construction_header = read("gba/include/tb/tower_construction_scene.h")
    construction_source = read("gba/src/tower_construction_scene.cpp")

    assert "void _show_modal_backdrop(int left, int top, int columns, int rows);" in build_city_header
    assert "void BuildCityScene::_show_modal_backdrop(int left, int top, int columns, int rows)" in build_city_source
    assert "_show_modal_backdrop(8, 32, 7, backdrop_rows);" in build_city_source
    assert "void _show_modal_backdrop(int line_count);" in construction_header
    assert "void TowerConstructionScene::_show_modal_backdrop(int line_count)" in construction_source
    assert "_show_modal_backdrop(line_count);" in construction_source


def test_dialogue_text_uses_roomier_line_spacing_inside_shared_window() -> None:
    build_city_source = read("gba/src/build_city_scene.cpp")
    construction_source = read("gba/src/tower_construction_scene.cpp")

    assert "constexpr int modal_line_spacing = 12;" in build_city_source
    assert "int y = -((line_count - 1) * modal_line_spacing) / 2;" in build_city_source
    assert "y += modal_line_spacing;" in build_city_source

    assert "constexpr int modal_line_spacing = 12;" in construction_source
    assert "int y = -((line_count - 1) * modal_line_spacing) / 2;" in construction_source
    assert "y += modal_line_spacing;" in construction_source


def test_current_block_is_layered_in_front_of_crane_and_special_cable() -> None:
    for relative in ("gba/src/quick_game_scene.cpp", "gba/src/tower_construction_scene.cpp"):
        source = read(relative)
        assert "constexpr int current_block_z_order = -20;" in source
        assert "constexpr int crane_mesh_z_order = -10;" in source
        assert "constexpr int special_cable_z_order = -5;" in source
        assert "sprite.set_z_order(current_block_z_order);" in source
        assert "sprite.set_z_order(crane_mesh_z_order);" in source
        assert "sprite.set_z_order(special_cable_z_order);" in source
