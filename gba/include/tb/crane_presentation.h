#ifndef TB_CRANE_PRESENTATION_H
#define TB_CRANE_PRESENTATION_H

#include <cstdint>

namespace tb
{
enum class CranePresentationMode : uint8_t
{
    Hidden = 0,
    Normal,
    Special,
};

enum class PerfectLandingSeamPhase : uint8_t
{
    Hidden = 0,
    White,
    Yellow,
};

inline constexpr int perfect_landing_star_count = 8;
inline constexpr int perfect_landing_star_duration_ms = 520;
inline constexpr int perfect_landing_star_trail_delay_ms = 80;
inline constexpr int perfect_landing_seam_white_ms = 50;
inline constexpr int perfect_landing_seam_total_ms = 130;
inline constexpr int perfect_landing_pattern_count = 10;

[[nodiscard]] constexpr int perfect_landing_pattern_bucket(int seed)
{
    const int normalized = seed < 0 ? -seed : seed;
    return normalized % perfect_landing_pattern_count;
}

[[nodiscard]] constexpr int perfect_landing_pattern_adjust_x(int index, int pattern)
{
    constexpr int table[perfect_landing_pattern_count][perfect_landing_star_count] = {
        { 0,  0,  0,  0,  0,  0,  0,  0},
        {-8, -5, -2,  0,  2,  5,  8, -4},
        { 8,  5,  2,  0, -2, -5, -8,  4},
        {-10, -7, -4,  0,  4,  7,  10,  0},
        { 10,  7,  4,  0, -4, -7, -10,  0},
        {-5, -2,  6, -5,  6, -2, -5,  4},
        { 5,  2, -6,  5, -6,  2,  5, -4},
        {-6,  2,  8, -2, -8, -2,  6,  2},
        { 6, -2, -8,  2,  8,  2, -6, -2},
        { 0, -6,  4, -8,  4,  6,  0,  8},
    };
    return table[pattern][index];
}

[[nodiscard]] constexpr int perfect_landing_pattern_adjust_y(int index, int pattern)
{
    constexpr int table[perfect_landing_pattern_count][perfect_landing_star_count] = {
        { 0,  0,  0,  0,  0,  0,  0,  0},
        {-4,  2,  8, -6,  8,  2, -4,  6},
        {-6,  4,  10, -2,  10,  4, -6,  2},
        { 2,  6,  12, -4,  12,  6,  2,  10},
        {-2,  10,  6, -8,  6,  10, -2,  12},
        {-8,  0,  10, -12,  10,  0, -8,  8},
        {-4,  8,  4, -10,  4,  8, -4,  12},
        { 0,  4,  12, -8,  12,  4,  0,  4},
        {-10,  4,  8, -4,  8,  4, -10,  10},
        {-2,  6,  14, -10,  14,  6, -2,  8},
    };
    return table[pattern][index];
}


[[nodiscard]] constexpr int perfect_landing_star_target_x(int index)
{
    switch(index)
    {
    case 0: return -46;
    case 1: return -34;
    case 2: return -20;
    case 3: return 0;
    case 4: return 20;
    case 5: return 34;
    case 6: return 46;
    case 7: return 0;
    default: return 0;
    }
}

[[nodiscard]] constexpr int perfect_landing_star_target_y(int index)
{
    switch(index)
    {
    case 0: return -22;
    case 1: return 8;
    case 2: return 30;
    case 3: return -40;
    case 4: return 30;
    case 5: return 8;
    case 6: return -22;
    case 7: return 38;
    default: return 0;
    }
}

[[nodiscard]] constexpr int perfect_landing_star_jitter_x(int index, int seed)
{
    const int value = ((seed + 3) * 17 + index * 11) & 7;
    return value - 3;
}

[[nodiscard]] constexpr int perfect_landing_star_jitter_y(int index, int seed)
{
    const int value = ((seed + 5) * 13 + index * 7) & 5;
    return value - 2;
}

[[nodiscard]] constexpr int perfect_landing_effect_elapsed(int elapsed_ms)
{
    if(elapsed_ms < 0)
    {
        return 0;
    }
    return elapsed_ms > perfect_landing_star_duration_ms ? perfect_landing_star_duration_ms : elapsed_ms;
}

[[nodiscard]] constexpr int perfect_landing_star_offset_x(int index, int elapsed_ms)
{
    const int elapsed = perfect_landing_effect_elapsed(elapsed_ms);
    return perfect_landing_star_target_x(index) * elapsed / perfect_landing_star_duration_ms;
}

[[nodiscard]] constexpr int perfect_landing_star_offset_y(int index, int elapsed_ms)
{
    const int elapsed = perfect_landing_effect_elapsed(elapsed_ms);
    return perfect_landing_star_target_y(index) * elapsed / perfect_landing_star_duration_ms;
}

[[nodiscard]] constexpr int perfect_landing_star_offset_x(int index, int elapsed_ms, int seed)
{
    const int elapsed = perfect_landing_effect_elapsed(elapsed_ms);
    const int pattern = perfect_landing_pattern_bucket(seed);
    const int target = perfect_landing_star_target_x(index) +
            perfect_landing_pattern_adjust_x(index, pattern) +
            perfect_landing_star_jitter_x(index, seed);
    return target * elapsed / perfect_landing_star_duration_ms;
}

[[nodiscard]] constexpr int perfect_landing_star_offset_y(int index, int elapsed_ms, int seed)
{
    const int elapsed = perfect_landing_effect_elapsed(elapsed_ms);
    const int pattern = perfect_landing_pattern_bucket(seed);
    const int target = perfect_landing_star_target_y(index) +
            perfect_landing_pattern_adjust_y(index, pattern) +
            perfect_landing_star_jitter_y(index, seed);
    return target * elapsed / perfect_landing_star_duration_ms;
}

[[nodiscard]] constexpr int perfect_landing_star_trail_elapsed(int elapsed_ms, int trail_index)
{
    const int delayed = elapsed_ms - (trail_index + 1) * perfect_landing_star_trail_delay_ms;
    return delayed > 0 ? delayed : 0;
}

[[nodiscard]] constexpr PerfectLandingSeamPhase perfect_landing_seam_phase(int elapsed_ms)
{
    if(elapsed_ms < 0 || elapsed_ms >= perfect_landing_seam_total_ms)
    {
        return PerfectLandingSeamPhase::Hidden;
    }
    return elapsed_ms < perfect_landing_seam_white_ms ?
            PerfectLandingSeamPhase::White : PerfectLandingSeamPhase::Yellow;
}

[[nodiscard]] constexpr CranePresentationMode crane_presentation_mode(
        bool playing, int floor_count, bool roof_phase, bool falling, bool missed)
{
    if(! playing)
    {
        return CranePresentationMode::Hidden;
    }
    if(roof_phase || missed)
    {
        return CranePresentationMode::Special;
    }
    if(floor_count == 0)
    {
        return falling ? CranePresentationMode::Hidden : CranePresentationMode::Special;
    }
    return CranePresentationMode::Normal;
}


inline constexpr int special_crane_fixed_start_y = -165;
inline constexpr int special_crane_intro_camera_start_y = 2432;
inline constexpr int special_crane_intro_camera_target_y = 512;
[[nodiscard]] constexpr int special_crane_cable_start_y(int camera_y, bool intro_camera_active)
{
    if(! intro_camera_active)
    {
        return special_crane_fixed_start_y;
    }
    const int camera_delta = special_crane_intro_camera_start_y - camera_y;
    return -(camera_delta * 22) / 256;
}

[[nodiscard]] constexpr int special_crane_boom_part_center_x(int cable_start_x, int part_index)
{
    return cable_start_x + (part_index == 0 ? -115 : part_index == 1 ? -51 : -3);
}

[[nodiscard]] constexpr int special_crane_boom_center_y(int cable_start_y)
{
    return cable_start_y - 15;
}

[[nodiscard]] constexpr int normal_floor_mesh_id(int building_type)
{
    return 9 + building_type;
}

[[nodiscard]] constexpr int initial_base_mesh_id(int building_type)
{
    return 19 + building_type;
}
}

#endif
