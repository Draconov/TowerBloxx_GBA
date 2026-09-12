from __future__ import annotations

from tools.tower_bloxx_extract.build_city_reference import (
    BUILDING_UNLOCK_MILESTONES,
    CITY_LEVEL_MILESTONES,
    MILESTONES,
    TARGET_HEIGHTS,
    TROPHY_UNLOCK_MILESTONES,
    placement_capability,
    progress_for_population,
)


def test_build_city_tables_match_m_and_house_bytecode() -> None:
    assert MILESTONES == (
        0, 75, 150, 250, 400, 600, 800, 1000, 1400, 1800, 2200,
        3000, 4000, 5000, 6500, 8000, 9500, 11500, 14000, 17000, 19000,
    )
    assert BUILDING_UNLOCK_MILESTONES == (0, 3, 6, 10)
    assert TROPHY_UNLOCK_MILESTONES == (8, 12, 14, 16)
    assert CITY_LEVEL_MILESTONES == (0, 1, 4, 7, 9, 11, 13, 15, 18, 20)
    assert TARGET_HEIGHTS == (10, 20, 30, 40)


def test_progress_for_population_matches_m_f_threshold_search() -> None:
    assert progress_for_population(0) == {
        "milestone": 0,
        "max_building_type": 0,
        "max_trophy_type": -1,
        "city_level": 0,
    }
    assert progress_for_population(249)["max_building_type"] == 0
    assert progress_for_population(250)["max_building_type"] == 1
    assert progress_for_population(799)["max_building_type"] == 1
    assert progress_for_population(800)["max_building_type"] == 2
    assert progress_for_population(2200)["max_building_type"] == 3
    assert progress_for_population(1399)["max_trophy_type"] == -1
    assert progress_for_population(1400)["max_trophy_type"] == 0
    assert progress_for_population(4000)["max_trophy_type"] == 1
    assert progress_for_population(74)["city_level"] == 0
    assert progress_for_population(75)["city_level"] == 1
    assert progress_for_population(19000)["city_level"] == 9


def test_placement_capability_uses_only_four_cardinal_neighbors() -> None:
    # m.g() inspects only left/right/up/down. Types are 0=empty, 1..4=towers.
    tiles = [0] * 25
    center = 2 + 2 * 5
    assert placement_capability(tiles, center) == 0

    tiles[center - 1] = 1
    assert placement_capability(tiles, center) == 1

    tiles[center + 1] = 2
    assert placement_capability(tiles, center) == 2

    tiles[center - 5] = 3
    assert placement_capability(tiles, center) == 3

    # Type 4 does not contribute to the three prerequisite color flags.
    tiles[center - 1] = 4
    tiles[center + 1] = 0
    tiles[center - 5] = 0
    tiles[center + 5] = 0
    tiles[center - 6] = 1  # diagonal must be ignored
    assert placement_capability(tiles, center) == 0


def test_edge_neighbor_checks_do_not_wrap_rows() -> None:
    tiles = [0] * 25
    # Index 5 is row 1, col 0; index 4 is diagonally across the row boundary.
    tiles[4] = 1
    assert placement_capability(tiles, 5) == 0
    tiles[6] = 1
    assert placement_capability(tiles, 5) == 1
