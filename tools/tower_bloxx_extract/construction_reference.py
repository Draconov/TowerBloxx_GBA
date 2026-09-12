from __future__ import annotations

TARGET_HEIGHTS = (10, 20, 30, 40)
SWING_X = (213, 256, 298, 341, 384)
SWING_Y = (85, 106, 128, 149, 170)
PERIODS = (1670, 1700, 1650, 1600, 1550, 1500, 1450)
ROOF_POPULATION_THRESHOLDS = (70, 250, 550, 1000)


def _family_index(building_type: int) -> int:
    if building_type < 1 or building_type > 4:
        raise ValueError(f"building type must be 1..4: {building_type}")
    return building_type


def _java_div(numerator: int, denominator: int) -> int:
    if denominator == 0:
        raise ZeroDivisionError
    quotient = abs(numerator) // abs(denominator)
    return -quotient if (numerator < 0) != (denominator < 0) else quotient


def construction_target_height(building_type: int) -> int:
    return TARGET_HEIGHTS[_family_index(building_type) - 1]


def construction_initial_period(building_type: int) -> int:
    family = _family_index(building_type)
    return PERIODS[family]


def roof_population_threshold(building_type: int) -> int:
    return ROOF_POPULATION_THRESHOLDS[_family_index(building_type) - 1]


def construction_difficulty(building_type: int, floors: int, target: int | None = None) -> tuple[int, int, int, int]:
    family = _family_index(building_type)
    if target is None:
        target = construction_target_height(building_type)
    if target <= 0:
        raise ValueError("target must be positive")

    period = PERIODS[family] - _java_div(floors * (PERIODS[family] - PERIODS[family + 2]), target)
    half_target = target >> 1
    swing_x = min(SWING_X[family], SWING_X[1] + _java_div(floors * (SWING_X[family] - SWING_X[1]), half_target))
    swing_y = min(SWING_Y[family], SWING_Y[1] + _java_div(floors * (SWING_Y[family] - SWING_Y[1]), half_target))
    vertical_bias = -min(128, _java_div(floors * 256, 100))
    return period, swing_x, swing_y, vertical_bias


def roof_bonus(building_type: int, absolute_offset: int, trophy: bool) -> int:
    family = _family_index(building_type)
    if absolute_offset < 0 or absolute_offset > 127:
        raise ValueError(f"absolute offset must be 0..127: {absolute_offset}")
    quality = 128 - absolute_offset
    if trophy:
        return _java_div(quality * family, 2)
    return _java_div(quality, 5 - family)
