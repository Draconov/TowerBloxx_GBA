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
