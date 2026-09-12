from __future__ import annotations

from collections.abc import Sequence

MILESTONES = (
    0, 75, 150, 250, 400, 600, 800, 1000, 1400, 1800, 2200,
    3000, 4000, 5000, 6500, 8000, 9500, 11500, 14000, 17000, 19000,
)
BUILDING_UNLOCK_MILESTONES = (0, 3, 6, 10)
TROPHY_UNLOCK_MILESTONES = (8, 12, 14, 16)
CITY_LEVEL_MILESTONES = (0, 1, 4, 7, 9, 11, 13, 15, 18, 20)
TARGET_HEIGHTS = (10, 20, 30, 40)


def _largest_index_not_above(values: Sequence[int], value: int) -> int:
    result = 0
    for index, threshold in enumerate(values):
        if value >= threshold:
            result = index
        else:
            break
    return result


def progress_for_population(population: int) -> dict[str, int]:
    milestone = _largest_index_not_above(MILESTONES, max(population, 0))

    max_building_type = 0
    for building_type, milestone_index in enumerate(BUILDING_UNLOCK_MILESTONES):
        if milestone >= milestone_index:
            max_building_type = building_type

    max_trophy_type = -1
    for building_type, milestone_index in enumerate(TROPHY_UNLOCK_MILESTONES):
        if milestone >= milestone_index:
            max_trophy_type = building_type

    city_level = 0
    for level, milestone_index in enumerate(CITY_LEVEL_MILESTONES):
        if milestone >= milestone_index:
            city_level = level

    return {
        "milestone": milestone,
        "max_building_type": max_building_type,
        "max_trophy_type": max_trophy_type,
        "city_level": city_level,
    }


def placement_capability(tile_types: Sequence[int], index: int) -> int:
    if len(tile_types) != 25:
        raise ValueError("Build City grid must contain exactly 25 tiles")
    if not 0 <= index < 25:
        raise IndexError(index)

    row, column = divmod(index, 5)
    neighbors: list[int] = []
    if column > 0:
        neighbors.append(index - 1)
    if column < 4:
        neighbors.append(index + 1)
    if row > 0:
        neighbors.append(index - 5)
    if row < 4:
        neighbors.append(index + 5)

    flags = [False, False, False]
    for neighbor in neighbors:
        building_type = int(tile_types[neighbor])
        if 1 <= building_type <= 3:
            flags[building_type - 1] = True

    if flags[0] and flags[1] and flags[2]:
        return 3
    if flags[0] and flags[1]:
        return 2
    if flags[0]:
        return 1
    return 0
