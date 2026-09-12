#ifndef TB_TOWER_CONSTRUCTION_H
#define TB_TOWER_CONSTRUCTION_H

#include <array>
#include <cstdint>

#include "tb/app_state.h"

namespace tb
{
enum class TowerConstructionStatus : uint8_t
{
    Playing = 0,
    Terminal,
    Results,
};

enum class TowerConstructionBlockState : uint8_t
{
    Attached = 1,
    Falling = 2,
    Missed = 3,
    Settled = 4,
    Slipping = 5,
    Raising = 6,
};

enum class TowerConstructionAccuracyBand : uint8_t
{
    None = 0,
    Ok = 1,
    Good = 2,
    Great = 3,
    Perfect = 4,
};

struct TowerConstructionFloor
{
    int x = 0;
    int y = 0;
    int offset = 0;
    bool roof = false;
};

struct TowerConstructionRenderPose
{
    int x_delta = 0;
    int y_delta = 0;
    int z_angle_degrees = 0;
};

struct TowerConstructionResult
{
    bool ready = false;
    uint8_t building_type = 0;
    int population = 0;
    uint8_t roof = 0;
};

struct TowerConstructionSnapshot
{
    TowerConstructionStatus status = TowerConstructionStatus::Playing;
    TowerConstructionBlockState block_state = TowerConstructionBlockState::Raising;
    TowerConstructionAccuracyBand last_accuracy = TowerConstructionAccuracyBand::None;
    uint8_t building_type = 1;
    int target_height = 10;
    int floor_count = 0;
    int chances_left = 3;
    int population = 0;
    bool roof_phase = false;
    bool trophy_requested = false;
    bool trophy_eligible = false;
    uint8_t roof_result = 0;
    int camera_y = 512;
    int camera_target_y = 512;
    int presentation_camera_y = 512;
    bool camera_impact_active = false;
    int current_x = 0;
    int current_y = 2432;
    int rope_length = 0;
    int swing_period_ms = 1700;
    int swing_amplitude_x = 128;
    int swing_amplitude_y = 64;
    int vertical_swing_bias = 0;
    int drop_velocity_x = 0;
    int drop_velocity_y = 0;
    int current_z_angle_degrees = 0;
    int crane_angle_degrees = 0;
    int combo_count = 0;
    int combo_bonus_pending = 0;
    int combo_meter_ms = -2000;
};

class TowerConstruction
{
public:
    static constexpr int stored_floor_count = 20;

    TowerConstruction();

    void start(uint8_t building_type, int target_height, bool trophy_eligible);
    void update(int delta_ms, const InputFrame& input);

    [[nodiscard]] TowerConstructionSnapshot snapshot() const;
    [[nodiscard]] TowerConstructionResult result() const;
    [[nodiscard]] int floor_count() const;
    [[nodiscard]] const TowerConstructionFloor& floor(int index) const;
    [[nodiscard]] const TowerConstructionRenderPose& floor_render_pose(int index) const;

#ifdef TB_HOST_TEST
    void debug_resolve_landing_for_test(int offset);
    void debug_register_miss_for_test();
#endif

private:
    std::array<TowerConstructionFloor, stored_floor_count> _floors{};
    std::array<TowerConstructionRenderPose, stored_floor_count> _floor_render_poses{};
    TowerConstructionStatus _status = TowerConstructionStatus::Playing;
    TowerConstructionBlockState _block_state = TowerConstructionBlockState::Raising;
    TowerConstructionAccuracyBand _last_accuracy = TowerConstructionAccuracyBand::None;
    uint8_t _building_type = 1;
    int _target_height = 10;
    int _floor_count = 0;
    int _chances_left = 3;
    int _population = 0;
    bool _roof_phase = false;
    bool _trophy_requested = false;
    bool _trophy_eligible = false;
    uint8_t _roof_result = 0;

    int _combo_count = 0;
    int _combo_bonus_pending = 0;
    int _combo_meter_ms = -2000;
    int _last_population_award = 0;

    int _accumulator_ms = 0;
    int _clock_ms = 0;
    int _swing_phase_ms = 2000;
    int _swing_period_ms = 1700;
    int _swing_amplitude_x = 128;
    int _swing_amplitude_y = 64;
    int _vertical_swing_bias = 0;
    int _world_anchor_y = 2432;
    int _rope_length = 0;

    int _crane_x = 0;
    int _crane_y = 2432;
    int _previous_crane_x = 0;
    int _previous_crane_y = 2432;
    int _current_x = 0;
    int _current_y = 2432;
    int _velocity_x = 0;
    int _velocity_y = 0;
    int _drop_velocity_x = 0;
    int _drop_velocity_y = 0;
    int _drop_start_x = 0;
    int _drop_start_y = 2432;
    int _drop_start_ms = 0;
    int _current_z_angle_degrees = 0;
    int _slip_target_z_angle_degrees = 0;

    int _camera_y = 512;
    int _camera_target_y = 512;
    int _camera_transition_start_ms = 0;
    int _presentation_camera_y = 512;
    int _camera_impact_start_ms = -1000000;
    uint64_t _visual_random_state = 0;
    int _transition_start_ms = 0;

    int _tower_phase_tenths = 0;
    int _tower_sway_wave = 0;
    int _tower_sway_amplitude = 0;
    int _tower_global_x = 0;
    int _tower_instability = 0;
    int _top_settle_start_ms = -1000000;
    int _top_settle_cached_angle = 0;

    void _start_drop();
    void _step(int delta_ms);
    void _update_camera();
    void _update_presentation(int delta_ms);
    void _update_tower_poses();
    int _next_visual_random(int bound);
    void _update_crane(int delta_ms);
    void _update_falling(int delta_ms);
    void _check_collision();
    void _resolve_landing(int floor_index, int floor_x);
    void _begin_slip(int offset);
    void _register_miss();
    void _award_population(int accuracy_points);
    void _settle_combo();
    void _update_combo(int delta_ms);
    void _add_floor(int offset, bool roof);
    void _enter_roof_phase_if_ready();
    void _update_difficulty();
    void _spawn_next_block();
    void _enter_terminal();
};
}

#endif
