#ifndef TB_QUICK_GAME_H
#define TB_QUICK_GAME_H

#include <array>
#include <cstdint>

#include "tb/app_state.h"

namespace tb
{
enum class QuickBlockState : uint8_t
{
    Attached = 1,
    Falling = 2,
    Missed = 3,
    Settled = 4,
    Slipping = 5,
    Raising = 6,
};

enum class QuickGameStatus : uint8_t
{
    Playing = 0,
    GameOver,
    Results,
};

enum class QuickAccuracyBand : uint8_t
{
    None = 0,
    Ok = 1,
    Good = 2,
    Great = 3,
    Perfect = 4,
};

struct QuickFloor
{
    int x = 0;
    int y = 0;
    int offset = 0;
};

struct QuickGameResult
{
    int population = 0;
    int height = 0;
    int longest_combo = 0;
};

struct QuickFloorRenderPose
{
    int x_delta = 0;
    int y_delta = 0;
    int z_angle_degrees = 0;
};

struct QuickGameSnapshot
{
    QuickGameStatus status = QuickGameStatus::Playing;
    QuickBlockState block_state = QuickBlockState::Raising;
    QuickAccuracyBand last_accuracy = QuickAccuracyBand::None;
    int floor_count = 0;
    int chances_left = 3;
    int camera_y = 512;
    int camera_target_y = 512;
    int presentation_camera_y = 512;
    bool camera_impact_active = false;
    int current_x = 0;
    int current_y = 2432;
    int crane_x = 0;
    int crane_y = 2432;
    int rope_length = 0;
    int swing_phase_ms = 2000;
    int swing_period_ms = 1550;
    int swing_amplitude_x = 128;
    int swing_amplitude_y = 64;
    int drop_velocity_x = 0;
    int drop_velocity_y = 0;
    int current_z_angle_degrees = 0;
    int crane_angle_degrees = 0;
    int tower_phase_tenths = 0;
    int tower_sway_wave = 0;
    int tower_sway_amplitude = 0;
    int tower_global_x = 0;
    int population = 0;
    int combo_count = 0;
    int combo_bonus_pending = 0;
    int combo_meter_ms = -2000;
    int longest_combo = 0;
    int last_population_award = 0;
};

class QuickGame
{
public:
    // The Java game stores a 20-entry cyclic floor-offset history. The GBA
    // runtime keeps the last 20 absolute floor records for collision/rendering.
    static constexpr int stored_floor_count = 20;

    QuickGame();

    void reset();
    void update(int delta_ms, const InputFrame& input);

    [[nodiscard]] QuickGameSnapshot snapshot() const;
    [[nodiscard]] QuickGameResult result() const;
    [[nodiscard]] int floor_count() const;
    [[nodiscard]] const QuickFloor& floor(int index) const;
    [[nodiscard]] const QuickFloorRenderPose& floor_render_pose(int index) const;

#ifdef TB_HOST_TEST
    void debug_resolve_landing_for_test(int offset);
    void debug_set_falling_state_for_test(int x, int y, int velocity_x, int velocity_y);
#endif

private:
    std::array<QuickFloor, stored_floor_count> _floors{};
    std::array<QuickFloorRenderPose, stored_floor_count> _floor_render_poses{};
    int _floor_count = 0;
    int _chances_left = 3;
    QuickGameStatus _status = QuickGameStatus::Playing;
    QuickBlockState _block_state = QuickBlockState::Raising;
    QuickAccuracyBand _last_accuracy = QuickAccuracyBand::None;
    int _population = 0;
    int _combo_count = 0;
    int _combo_bonus_pending = 0;
    int _combo_meter_ms = -2000;
    int _longest_combo = 0;
    int _last_population_award = 0;

    int _accumulator_ms = 0;
    int _clock_ms = 0;
    int _swing_phase_ms = 2000;
    int _swing_period_ms = 1550;
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
    void _add_floor(int offset);
    void _update_difficulty();
    void _spawn_next_block();
};
}

#endif
