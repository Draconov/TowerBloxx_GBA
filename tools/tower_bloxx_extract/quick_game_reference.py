"""Recovered Tower Bloxx Quick Game constants and Java integer trig helpers.

The values in this module are transcription targets for the native GBA runtime.
They intentionally preserve the Java ME game's integer arithmetic/sign conventions.
"""

from __future__ import annotations

QUICK_GAME_CONSTANTS = {
    "swing_x": (213, 256, 298, 341, 384),
    "swing_y": (85, 106, 128, 149, 170),
    "period_ms": (1670, 1700, 1650, 1600, 1550, 1500, 1450),
    "target_floors": (10, 20, 30, 40),
    "initial_camera": 512,
    "initial_world_anchor": 2432,
    "initial_rope_length": 0,
    "max_rope_length": 1664,
    "landing_max_abs_offset": 127,
    "accuracy_thresholds": (25, 50, 80),
    "initial_chances": 3,
    "next_block_delay_ms": 400,
    "physics_gate_ms": 25,
    "max_frame_delta_ms": 150,
    # House mode setup: Quick Game takes the non-Build-City branch and fixes L=4.
    "mode_index": 4,
    "initial_period_ms": 1550,
    # The 10/20/30/40 array belongs to construction progression; Quick Game has no height completion.
    "quick_game_completion_floor": None,
    # House.b()/House.a(int,int)/House.r() combo contract.
    "combo_start_ms": 6000,
    "combo_idle_floor_ms": -2000,
    # House.m(int) + main update: third miss enters K=2, result popup after 2000 ms.
    "result_delay_ms": 2000,
    # House.b() -> d(points, Q+1) -> k(points).
    "population_accuracy_points": {
        "ok": 1,
        "good": 2,
        "great": 3,
        "perfect": 4,
    },
}


def build_java_sine_table() -> tuple[int, ...]:
    """Reproduce ``House.w()``'s 360-entry Q15-ish sine recurrence."""

    sin_value = 0
    cos_value = 32768
    step = (32768 * 31416 * 2) // 3600000
    output: list[int] = []

    for _ in range(360):
        output.append(sin_value)
        cos_value = cos_value - ((sin_value * step) >> 15)
        sin_value = sin_value + ((cos_value * step) >> 15)

    return tuple(output)


_JAVA_SINE_TABLE = build_java_sine_table()


def java_sin_u15(angle_degrees: int) -> int:
    """Reproduce ``House.b(int)`` for its bytecode-proven -359..359 domain."""

    if angle_degrees < -359 or angle_degrees > 359:
        raise ValueError("House.b(int) indexes the 360-entry table directly")
    if angle_degrees < 0:
        return -_JAVA_SINE_TABLE[-angle_degrees]
    return _JAVA_SINE_TABLE[angle_degrees]


def java_cos_u15(angle_degrees: int) -> int:
    """Reproduce ``House.c(int)``: ``House.b(angle - 90)``."""

    shifted = angle_degrees - 90
    if shifted < -359 or shifted > 359:
        raise ValueError("angle is outside the recovered House.c(int) lookup domain")
    return java_sin_u15(shifted)


def quick_game_difficulty(floor_count: int) -> dict[str, int]:
    """Reproduce the ``B != 3`` branch of ``House.l(int)``.

    ``floor_count`` is ``Q`` after the just-landed floor has been committed.
    Java integer division is truncation toward zero; all divisors here have
    positive operands, so Python ``//`` is equivalent.
    """

    if floor_count < 0:
        raise ValueError("floor_count must be non-negative")

    mode = QUICK_GAME_CONSTANTS["mode_index"]
    swing_x = QUICK_GAME_CONSTANTS["swing_x"]
    swing_y = QUICK_GAME_CONSTANTS["swing_y"]
    periods = QUICK_GAME_CONSTANTS["period_ms"]

    amp_x = min(swing_x[mode], swing_x[0] + floor_count * (swing_x[mode] - swing_x[0]) // 60)
    amp_y = min(swing_y[mode], swing_y[0] + floor_count * (swing_y[mode] - swing_y[0]) // 60)

    if floor_count < 100:
        vertical_bias = -min(128, floor_count * 256 // 200)
        period = max(
            periods[mode + 2],
            periods[0] - floor_count * (periods[0] - periods[mode + 2]) // 100,
        )
    else:
        period = periods[mode + 1] - (floor_count - 100) * 100 // 150
        vertical_bias = -min(256, 128 + (floor_count - 100) * 256 // 300)

    return {
        "swing_x": amp_x,
        "swing_y": amp_y,
        "vertical_bias": vertical_bias,
        "period_ms": period,
    }


def population_base_award(floor_count: int, accuracy_points: int) -> int:
    """Reproduce the immediate ``R += Q / 10 + points`` part of ``House.k(int)``."""

    if floor_count < 0 or accuracy_points <= 0:
        raise ValueError("expected a non-negative floor count and positive accuracy points")
    return floor_count // 10 + accuracy_points


def combo_pending_bonus(floor_count: int, combo_count: int) -> int:
    """Reproduce the deferred combo addition to ``Z`` in ``House.k(int)``."""

    if floor_count < 0 or combo_count <= 0:
        raise ValueError("expected a non-negative floor count and positive combo count")
    return combo_count * (2 + 2 * (floor_count // 10))


def combo_drain_ms(delta_ms: int, combo_count: int) -> int:
    """Reproduce ``dt + (X - 1) * dt / 6`` from ``House.a(int,int)``."""

    if delta_ms < 0 or combo_count <= 0:
        raise ValueError("expected a non-negative delta and positive combo count")
    return delta_ms + (combo_count - 1) * delta_ms // 6


def java_div(numerator: int, denominator: int) -> int:
    """Java integer division (truncate toward zero) for signed presentation math."""

    if denominator == 0:
        raise ZeroDivisionError("division by zero")
    sign = -1 if (numerator < 0) != (denominator < 0) else 1
    return sign * (abs(numerator) // abs(denominator))


def tower_sway_state(floor_offsets: tuple[int, ...], phase_tenths: int) -> dict[str, int]:
    """Reproduce the Quick Game ``House.l(int)`` + ``House.f(int)`` sway state.

    ``phase_tenths`` is the recovered ``U`` accumulator in tenths of a degree
    (0..3599). Only the last five floor offsets contribute to ``aL``.
    """

    if phase_tenths < 0:
        raise ValueError("phase_tenths must be non-negative")

    floor_count = len(floor_offsets)
    last_five = floor_offsets[max(0, floor_count - 5):]
    instability = sum(abs(value) for value in last_five) // 5
    instability = min(java_div(floor_count * instability, 20), 100)

    cumulative_offset = sum(floor_offsets)
    base = floor_count // 2 + abs(cumulative_offset) // 20
    amplitude = min(base, java_div(floor_count * base, 6))

    phase = phase_tenths % 3600
    wave = java_cos_u15(phase // 10)
    global_x = java_div(-wave * amplitude, 10000)
    return {
        "instability": instability,
        "amplitude": amplitude,
        "wave": wave,
        "global_x": global_x,
    }


def top_settle_angle(
    *, base_angle: int, offset: int, elapsed_ms: int, cached_angle: int
) -> tuple[int, int]:
    """Reproduce the top-floor 0..800ms settling branch inside ``House.q()``."""

    if elapsed_ms < 0:
        raise ValueError("elapsed_ms must be non-negative")

    if elapsed_ms < 100:
        angle = java_div(base_angle, 8) + java_div(offset, 6)
        angle += java_div(elapsed_ms * offset, 600)
        return angle, cached_angle

    if elapsed_ms < 500:
        angle = java_div(base_angle, 8) + java_div(offset, 6)
        angle += java_div((500 - elapsed_ms) * offset, 2400)
        return angle, angle

    if elapsed_ms < 800:
        angle = cached_angle - java_div((cached_angle - base_angle) * (elapsed_ms - 500), 300)
        return angle, cached_angle

    return base_angle, cached_angle


def _sway_floor_correction(instability: int, offset: int, wave: int) -> int:
    if wave > 0:
        if offset < 0:
            return java_div(instability * offset * wave, 29491200)
        return java_div(-instability * offset * wave, 58982400)

    if offset > 0:
        return java_div(-instability * offset * wave, 29491200)
    return java_div(instability * offset * wave, 58982400)


def tower_pose_deltas(
    floor_offsets: tuple[int, ...],
    phase_tenths: int,
    *,
    settle_elapsed_ms: int | None = None,
    cached_top_angle: int = 0,
) -> list[dict[str, int]]:
    """Return GBA-friendly deltas for the up-to-five floors rendered by ``House.q()``.

    The original routine builds absolute transforms from its own rolling base.
    The port already stores collision-accurate absolute floor coordinates, so
    this helper returns only the recovered sway/rocking displacement and Z angle
    to layer on top of those coordinates.
    """

    if not floor_offsets:
        return []

    state = tower_sway_state(floor_offsets, phase_tenths)
    instability = state["instability"]
    wave = state["wave"]
    delta_x = state["global_x"]
    delta_y = 0
    running_angle = 0
    first_visible = max(0, len(floor_offsets) - 5)
    output: list[dict[str, int]] = []

    for floor_index in range(first_visible, len(floor_offsets)):
        offset = floor_offsets[floor_index]
        is_top = floor_index == len(floor_offsets) - 1
        if is_top and settle_elapsed_ms is not None:
            running_angle, cached_top_angle = top_settle_angle(
                base_angle=running_angle,
                offset=offset,
                elapsed_ms=settle_elapsed_ms,
                cached_angle=cached_top_angle,
            )

        running_angle += _sway_floor_correction(instability, offset, wave)
        delta_x += 2 * running_angle
        half_angle = running_angle >> 1
        delta_y += half_angle if wave > 0 else -half_angle

        output.append({
            "floor_index": floor_index,
            "angle": 0 if floor_index == 0 else running_angle,
            "dx": delta_x,
            "dy": delta_y,
        })

    return output


def camera_impact_offset(random_0_63: int, *, elapsed_ms: int) -> int:
    """Reproduce the 800ms ``y += 32 - a(64)`` camera-impact branch in ``House.p()``."""

    if random_0_63 < 0 or random_0_63 > 63:
        raise ValueError("random_0_63 must be in 0..63")
    if elapsed_ms < 0:
        raise ValueError("elapsed_ms must be non-negative")
    return 32 - random_0_63 if elapsed_ms < 800 else 0
