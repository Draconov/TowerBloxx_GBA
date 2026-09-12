from __future__ import annotations

from tools.tower_bloxx_extract.construction_reference import (
    construction_difficulty,
    construction_initial_period,
    construction_target_height,
    roof_bonus,
    roof_population_threshold,
)


def test_family_targets_periods_and_roof_population_thresholds() -> None:
    assert [construction_target_height(i) for i in range(1, 5)] == [10, 20, 30, 40]
    assert [construction_initial_period(i) for i in range(1, 5)] == [1700, 1650, 1600, 1550]
    assert [roof_population_threshold(i) for i in range(1, 5)] == [70, 250, 550, 1000]


def test_construction_difficulty_matches_family_tables_and_java_integer_math() -> None:
    # Before/at the first placed floor the family-1 values stay at their table baseline.
    assert construction_difficulty(1, 1, 10) == (1690, 256, 106, -2)
    # Type 4 ramps toward the family-specific amplitudes while period moves toward m[L+2].
    assert construction_difficulty(4, 20, 40) == (1500, 384, 170, -51)
    assert construction_difficulty(4, 39, 40) == (1453, 384, 170, -99)


def test_roof_bonus_matches_normal_and_trophy_formulas() -> None:
    assert [roof_bonus(i, 0, False) for i in range(1, 5)] == [32, 42, 64, 128]
    assert [roof_bonus(i, 0, True) for i in range(1, 5)] == [64, 128, 192, 256]
    assert roof_bonus(2, 27, False) == 33
    assert roof_bonus(3, 27, True) == 151
