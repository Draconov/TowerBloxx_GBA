from __future__ import annotations

from tools.tower_bloxx_extract.quick_game_reference import (
    QUICK_GAME_CONSTANTS,
    build_java_sine_table,
    java_cos_u15,
    java_sin_u15,
)


def test_quick_game_constants_match_recovered_house_bytecode() -> None:
    assert QUICK_GAME_CONSTANTS["swing_x"] == (213, 256, 298, 341, 384)
    assert QUICK_GAME_CONSTANTS["swing_y"] == (85, 106, 128, 149, 170)
    assert QUICK_GAME_CONSTANTS["period_ms"] == (1670, 1700, 1650, 1600, 1550, 1500, 1450)
    assert QUICK_GAME_CONSTANTS["target_floors"] == (10, 20, 30, 40)
    assert QUICK_GAME_CONSTANTS["initial_camera"] == 512
    assert QUICK_GAME_CONSTANTS["initial_world_anchor"] == 2432
    assert QUICK_GAME_CONSTANTS["initial_rope_length"] == 0
    assert QUICK_GAME_CONSTANTS["max_rope_length"] == 1664
    assert QUICK_GAME_CONSTANTS["landing_max_abs_offset"] == 127
    assert QUICK_GAME_CONSTANTS["accuracy_thresholds"] == (25, 50, 80)
    assert QUICK_GAME_CONSTANTS["initial_chances"] == 3
    assert QUICK_GAME_CONSTANTS["next_block_delay_ms"] == 400
    assert QUICK_GAME_CONSTANTS["physics_gate_ms"] == 25
    assert QUICK_GAME_CONSTANTS["max_frame_delta_ms"] == 150


def test_java_integer_sine_table_matches_pinned_samples() -> None:
    table = build_java_sine_table()
    assert len(table) == 360
    expected = {
        0: 0,
        1: 571,
        30: 16352,
        45: 23132,
        60: 28342,
        90: 32771,
        120: 28453,
        180: 222,
        270: -32714,
        359: -897,
    }
    for angle, value in expected.items():
        assert table[angle] == value
        assert java_sin_u15(angle) == value


def test_java_cos_helper_reproduces_house_c_shift_and_negative_lookup() -> None:
    expected = {
        0: -32771,
        30: -28342,
        60: -16352,
        90: 0,
        120: 16352,
        180: 32771,
        270: 222,
        359: -32704,
    }
    for angle, value in expected.items():
        assert java_cos_u15(angle) == value

    assert java_sin_u15(-30) == -16352
    assert java_sin_u15(-90) == -32771


def test_quick_game_mode_scoring_and_result_constants_match_bytecode() -> None:
    assert QUICK_GAME_CONSTANTS["mode_index"] == 4
    assert QUICK_GAME_CONSTANTS["initial_period_ms"] == 1550
    assert QUICK_GAME_CONSTANTS["quick_game_completion_floor"] is None
    assert QUICK_GAME_CONSTANTS["combo_start_ms"] == 6000
    assert QUICK_GAME_CONSTANTS["combo_idle_floor_ms"] == -2000
    assert QUICK_GAME_CONSTANTS["result_delay_ms"] == 2000
    assert QUICK_GAME_CONSTANTS["population_accuracy_points"] == {
        "ok": 1,
        "good": 2,
        "great": 3,
        "perfect": 4,
    }


def test_quick_game_endless_difficulty_matches_recovered_java_integer_math() -> None:
    from tools.tower_bloxx_extract.quick_game_reference import quick_game_difficulty

    assert quick_game_difficulty(0) == {
        "swing_x": 213,
        "swing_y": 85,
        "vertical_bias": 0,
        "period_ms": 1670,
    }
    assert quick_game_difficulty(1) == {
        "swing_x": 215,
        "swing_y": 86,
        "vertical_bias": -1,
        "period_ms": 1668,
    }
    assert quick_game_difficulty(60) == {
        "swing_x": 384,
        "swing_y": 170,
        "vertical_bias": -76,
        "period_ms": 1538,
    }
    assert quick_game_difficulty(99) == {
        "swing_x": 384,
        "swing_y": 170,
        "vertical_bias": -126,
        "period_ms": 1453,
    }
    assert quick_game_difficulty(100) == {
        "swing_x": 384,
        "swing_y": 170,
        "vertical_bias": -128,
        "period_ms": 1500,
    }
    assert quick_game_difficulty(102) == {
        "swing_x": 384,
        "swing_y": 170,
        "vertical_bias": -129,
        "period_ms": 1499,
    }


def test_population_and_combo_helpers_match_house_k_r_and_update() -> None:
    from tools.tower_bloxx_extract.quick_game_reference import (
        combo_drain_ms,
        combo_pending_bonus,
        population_base_award,
    )

    assert population_base_award(0, 4) == 4
    assert population_base_award(9, 1) == 1
    assert population_base_award(10, 1) == 2
    assert population_base_award(20, 3) == 5
    assert combo_pending_bonus(0, 1) == 2
    assert combo_pending_bonus(0, 2) == 4
    assert combo_pending_bonus(10, 3) == 12
    assert combo_drain_ms(25, 1) == 25
    assert combo_drain_ms(25, 2) == 29
    assert combo_drain_ms(25, 4) == 37
