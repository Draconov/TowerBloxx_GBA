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


def test_tower_sway_amplitude_and_phase_match_house_l_and_f() -> None:
    from tools.tower_bloxx_extract.quick_game_reference import tower_sway_state

    # Five offsets with cumulative lean 60. House.l(int) divides the last-five
    # absolute offset sum by 5, scales it by Q/20, then derives T from Q and M.
    state = tower_sway_state((0, 10, 20, -30, 60), phase_tenths=0)
    assert state == {
        "instability": 6,
        "amplitude": 4,
        "wave": -32771,
        "global_x": 13,
    }

    quarter = tower_sway_state((0, 10, 20, -30, 60), phase_tenths=900)
    assert quarter["wave"] == 0
    assert quarter["global_x"] == 0


def test_top_floor_settle_wobble_matches_house_q_piecewise_windows() -> None:
    from tools.tower_bloxx_extract.quick_game_reference import top_settle_angle

    # Use a non-zero base angle to prove the 500..800 ms branch interpolates
    # back toward the normal tower angle rather than toward zero.
    angle, cache = top_settle_angle(base_angle=12, offset=60, elapsed_ms=50, cached_angle=0)
    assert (angle, cache) == (12 // 8 + 60 // 6 + (50 * 60) // 600, 0)

    angle, cache = top_settle_angle(base_angle=12, offset=60, elapsed_ms=100, cached_angle=0)
    assert (angle, cache) == (12 // 8 + 60 // 6 + (400 * 60) // 2400, 21)

    angle, cache = top_settle_angle(base_angle=12, offset=60, elapsed_ms=475, cached_angle=21)
    assert (angle, cache) == (12 // 8 + 60 // 6 + (25 * 60) // 2400, 11)

    angle, cache = top_settle_angle(base_angle=12, offset=60, elapsed_ms=650, cached_angle=11)
    assert (angle, cache) == (11, 11)

    assert top_settle_angle(base_angle=12, offset=60, elapsed_ms=800, cached_angle=11) == (12, 11)


def test_tower_pose_deltas_accumulate_rotation_displacement_like_house_q() -> None:
    from tools.tower_bloxx_extract.quick_game_reference import tower_pose_deltas

    poses = tower_pose_deltas((0, 32, -48, 24, 60), phase_tenths=1800)
    assert len(poses) == 5
    # Foundation is the Java special sentinel (-999): no visible Z rotation.
    assert poses[0]["angle"] == 0
    # Every later X pose includes twice the running angle; Y accumulates half
    # the running angle with sign selected by the sway wave.
    assert poses == [
        {"floor_index": 0, "angle": 0, "dx": -13, "dy": 0},
        {"floor_index": 1, "angle": 0, "dx": -13, "dy": 0},
        {"floor_index": 2, "angle": 0, "dx": -13, "dy": 0},
        {"floor_index": 3, "angle": 0, "dx": -13, "dy": 0},
        {"floor_index": 4, "angle": 0, "dx": -13, "dy": 0},
    ]


def test_camera_impact_jitter_matches_house_p_range_and_window() -> None:
    from tools.tower_bloxx_extract.quick_game_reference import camera_impact_offset

    assert camera_impact_offset(0, elapsed_ms=0) == 32
    assert camera_impact_offset(63, elapsed_ms=799) == -31
    assert camera_impact_offset(17, elapsed_ms=800) == 0
