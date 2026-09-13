#include "tb/tower_construction.h"

#include <array>
#include <cassert>
#include <cstdint>

namespace tb
{
namespace
{
constexpr std::array<int, 5> swing_x_table = {213, 256, 298, 341, 384};
constexpr std::array<int, 5> swing_y_table = {85, 106, 128, 149, 170};
constexpr std::array<int, 7> period_table = {1670, 1700, 1650, 1600, 1550, 1500, 1450};
constexpr std::array<int, 4> target_table = {10, 20, 30, 40};
constexpr std::array<int, 4> trophy_population_table = {70, 250, 550, 1000};
constexpr int max_rope_length = 1664;
constexpr int fixed_floor_height = 256;
constexpr int first_floor_center_y = 128;
constexpr int landing_max_abs_offset = 127;
constexpr int next_block_delay_ms = 400;
constexpr int camera_transition_ms = 500;
constexpr int terminal_delay_ms = 2000;
constexpr int view_height_fixed = (256 * 160) / 22;
constexpr int view_half_fixed = view_height_fixed / 2;
constexpr int camera_impact_ms = 800;
constexpr uint64_t java_random_multiplier = 0x5DEECE66DULL;
constexpr uint64_t java_random_addend = 0xBULL;
constexpr uint64_t java_random_mask = (uint64_t(1) << 48) - 1;
constexpr uint64_t visual_random_seed = (0x4349545954ULL ^ java_random_multiplier) & java_random_mask;

constexpr std::array<int, 360> make_sine_table()
{
    std::array<int, 360> result{};
    int sin_value = 0;
    int cos_value = 32768;
    constexpr int64_t step = (int64_t(32768) * 31416 * 2) / 3600000;

    for(int index = 0; index < 360; ++index)
    {
        result[index] = sin_value;
        cos_value = int(int64_t(cos_value) - ((int64_t(sin_value) * step) >> 15));
        sin_value = int(int64_t(sin_value) + ((int64_t(cos_value) * step) >> 15));
    }

    return result;
}

constexpr auto sine_table = make_sine_table();

constexpr int java_sin(int angle)
{
    return angle < 0 ? -sine_table[-angle] : sine_table[angle];
}

constexpr int java_cos(int angle)
{
    return java_sin(angle - 90);
}

constexpr int clamp_frame_delta(int delta_ms)
{
    if(delta_ms < 0)
    {
        return 0;
    }
    return delta_ms > 150 ? 150 : delta_ms;
}

constexpr int abs_value(int value)
{
    return value < 0 ? -value : value;
}

constexpr int min_value(int left, int right)
{
    return left < right ? left : right;
}

constexpr int max_value(int left, int right)
{
    return left > right ? left : right;
}

constexpr int java_shift_right_one(int value)
{
    return value >= 0 ? value / 2 : -((-value + 1) / 2);
}

constexpr TowerConstructionAccuracyBand accuracy_for_offset(int offset)
{
    const int absolute = abs_value(offset);
    if(absolute < 25)
    {
        return TowerConstructionAccuracyBand::Perfect;
    }
    if(absolute < 50)
    {
        return TowerConstructionAccuracyBand::Great;
    }
    if(absolute < 80)
    {
        return TowerConstructionAccuracyBand::Good;
    }
    return TowerConstructionAccuracyBand::Ok;
}
}

TowerConstruction::TowerConstruction()
{
    start(1, 10, false);
}

void TowerConstruction::start(uint8_t building_type, int target_height, bool trophy_eligible)
{
    assert(building_type >= 1 && building_type <= 4);
    assert(target_height == target_table[building_type - 1]);

    _floors = {};
    _floor_render_poses = {};
    _status = TowerConstructionStatus::Playing;
    _block_state = TowerConstructionBlockState::Raising;
    _last_accuracy = TowerConstructionAccuracyBand::None;
    _building_type = building_type;
    _target_height = target_height;
    _floor_count = 0;
    _chances_left = 3;
    _population = 0;
    _roof_phase = false;
    _trophy_requested = trophy_eligible;
    _trophy_eligible = trophy_eligible;
    _roof_result = 0;

    _combo_count = 0;
    _combo_bonus_pending = 0;
    _combo_meter_ms = -2000;
    _last_population_award = 0;

    _accumulator_ms = 0;
    _clock_ms = 0;
    _swing_phase_ms = 2000;
    _swing_period_ms = period_table[building_type];
    _swing_amplitude_x = 128;
    _swing_amplitude_y = 64;
    _vertical_swing_bias = 0;
    _world_anchor_y = 2432;
    _rope_length = 0;

    _crane_x = 0;
    _crane_y = 2432;
    _previous_crane_x = 0;
    _previous_crane_y = 2432;
    _current_x = 0;
    _current_y = 2432;
    _velocity_x = 0;
    _velocity_y = 0;
    _drop_velocity_x = 0;
    _drop_velocity_y = 0;
    _drop_start_x = 0;
    _drop_start_y = 2432;
    _drop_start_ms = 0;
    _current_z_angle_degrees = 0;
    _slip_target_z_angle_degrees = 0;

    _camera_y = 512;
    _camera_target_y = 512;
    _camera_transition_start_ms = 0;
    _presentation_camera_y = 512;
    _camera_impact_start_ms = -1000000;
    _visual_random_state = (visual_random_seed + building_type) & java_random_mask;
    _transition_start_ms = 0;

    _tower_phase_tenths = 0;
    _tower_sway_wave = java_cos(0);
    _tower_sway_amplitude = 0;
    _tower_global_x = 0;
    _tower_instability = 0;
    _top_settle_start_ms = -1000000;
    _top_settle_cached_angle = 0;
}

void TowerConstruction::update(int delta_ms, const InputFrame& input)
{
    if(_status == TowerConstructionStatus::Results)
    {
        return;
    }

    if(_status == TowerConstructionStatus::Playing && input.pressed(Key::A) &&
       _block_state == TowerConstructionBlockState::Attached && _camera_y == _camera_target_y)
    {
        _start_drop();
    }

    _accumulator_ms += clamp_frame_delta(delta_ms);
    if(_accumulator_ms >= 25)
    {
        const int step_ms = _accumulator_ms;
        _accumulator_ms = 0;
        if(_status == TowerConstructionStatus::Terminal)
        {
            _clock_ms += step_ms;
            _update_presentation(step_ms);
            if(_clock_ms - _transition_start_ms >= terminal_delay_ms)
            {
                _status = TowerConstructionStatus::Results;
            }
        }
        else
        {
            _step(step_ms);
        }
    }
}

TowerConstructionSnapshot TowerConstruction::snapshot() const
{
    TowerConstructionSnapshot result;
    result.status = _status;
    result.block_state = _block_state;
    result.last_accuracy = _last_accuracy;
    result.building_type = _building_type;
    result.target_height = _target_height;
    result.floor_count = _floor_count;
    result.chances_left = _chances_left;
    result.population = _population;
    result.roof_phase = _roof_phase;
    result.trophy_requested = _trophy_requested;
    result.trophy_eligible = _trophy_eligible;
    result.roof_result = _roof_result;
    result.camera_y = _camera_y;
    result.camera_target_y = _camera_target_y;
    result.presentation_camera_y = _presentation_camera_y;
    result.camera_impact_active = _clock_ms - _camera_impact_start_ms < camera_impact_ms;
    result.current_x = _current_x;
    result.current_y = _current_y;
    result.crane_x = _crane_x;
    result.crane_y = _crane_y;
    result.rope_length = _rope_length;
    result.swing_period_ms = _swing_period_ms;
    result.swing_amplitude_x = _swing_amplitude_x;
    result.swing_amplitude_y = _swing_amplitude_y;
    result.vertical_swing_bias = _vertical_swing_bias;
    result.drop_velocity_x = _drop_velocity_x;
    result.drop_velocity_y = _drop_velocity_y;
    result.current_z_angle_degrees = _current_z_angle_degrees;
    result.crane_angle_degrees = ((_crane_x >> 4) * 2) / 3;
    result.combo_count = _combo_count;
    result.combo_bonus_pending = _combo_bonus_pending;
    result.combo_meter_ms = _combo_meter_ms;
    return result;
}

TowerConstructionResult TowerConstruction::result() const
{
    return TowerConstructionResult{_status == TowerConstructionStatus::Results, _building_type, _population, _roof_result};
}

int TowerConstruction::floor_count() const
{
    return _floor_count;
}

const TowerConstructionFloor& TowerConstruction::floor(int index) const
{
    const int first_stored = max_value(0, _floor_count - stored_floor_count);
    assert(index >= first_stored && index < _floor_count);
    return _floors[index % stored_floor_count];
}

const TowerConstructionRenderPose& TowerConstruction::floor_render_pose(int index) const
{
    const int first_stored = max_value(0, _floor_count - stored_floor_count);
    assert(index >= first_stored && index < _floor_count);
    return _floor_render_poses[index % stored_floor_count];
}

#ifdef TB_HOST_TEST
void TowerConstruction::debug_resolve_landing_for_test(int offset)
{
    assert(_status == TowerConstructionStatus::Playing);
    const int floor_x = _floor_count == 0 ? 0 : floor(_floor_count - 1).x;
    _current_x = floor_x + offset;
    _current_y = _floor_count == 0 ? 0 : floor(_floor_count - 1).y + 1;
    _block_state = TowerConstructionBlockState::Falling;
    _resolve_landing(_floor_count - 1, floor_x);
}

void TowerConstruction::debug_register_miss_for_test()
{
    if(_status == TowerConstructionStatus::Playing)
    {
        _block_state = TowerConstructionBlockState::Falling;
        _register_miss();
    }
}
#endif

void TowerConstruction::_start_drop()
{
    _block_state = TowerConstructionBlockState::Falling;
    _drop_start_x = _current_x;
    _drop_start_y = _current_y;
    _drop_start_ms = _clock_ms;
    _drop_velocity_x = _velocity_x;
    _drop_velocity_y = _velocity_y;
}

void TowerConstruction::_step(int delta_ms)
{
    _clock_ms += delta_ms;
    _update_camera();
    _update_crane(delta_ms);

    if(_block_state == TowerConstructionBlockState::Raising || _block_state == TowerConstructionBlockState::Attached)
    {
        _current_x = _crane_x;
        _current_y = _crane_y;
    }
    else if(_block_state == TowerConstructionBlockState::Falling || _block_state == TowerConstructionBlockState::Slipping)
    {
        _update_falling(delta_ms);
    }
    else if(_block_state == TowerConstructionBlockState::Settled || _block_state == TowerConstructionBlockState::Missed)
    {
        if(_clock_ms - _transition_start_ms >= next_block_delay_ms)
        {
            _spawn_next_block();
        }
    }

    _update_combo(delta_ms);
    _update_presentation(delta_ms);
}

void TowerConstruction::_update_camera()
{
    if(_camera_y < _camera_target_y)
    {
        const int elapsed = _clock_ms - _camera_transition_start_ms;
        const int candidate = _camera_target_y + ((elapsed - camera_transition_ms) * 256) / camera_transition_ms;
        _camera_y = min_value(_camera_target_y, candidate);
    }
    else if(_camera_y > _camera_target_y)
    {
        const int elapsed = _clock_ms - _camera_transition_start_ms;
        const int candidate = _camera_target_y - ((elapsed - camera_transition_ms) * 256) / camera_transition_ms;
        _camera_y = max_value(_camera_target_y, candidate);
    }

    _world_anchor_y = _camera_y + 1792 + 128;
}

void TowerConstruction::_update_presentation(int delta_ms)
{
    if(_status == TowerConstructionStatus::Terminal)
    {
        _tower_sway_wave = 0;
    }
    else
    {
        _tower_phase_tenths = (_tower_phase_tenths + delta_ms) % 3600;
        _tower_sway_wave = java_cos(_tower_phase_tenths / 10);
    }
    _update_tower_poses();

    _presentation_camera_y = _camera_y;
    const int impact_elapsed = _clock_ms - _camera_impact_start_ms;
    if(impact_elapsed >= 0 && impact_elapsed < camera_impact_ms)
    {
        _presentation_camera_y += 32 - _next_visual_random(64);
    }
}

void TowerConstruction::_update_tower_poses()
{
    if(_floor_count == 0)
    {
        _tower_instability = 0;
        _tower_sway_amplitude = 0;
        _tower_global_x = 0;
        return;
    }

    const int first_visible = max_value(0, _floor_count - 5);
    int absolute_sum = 0;
    for(int index = first_visible; index < _floor_count; ++index)
    {
        absolute_sum += abs_value(floor(index).offset);
    }
    const int average_offset = absolute_sum / 5;
    _tower_instability = min_value((_floor_count * average_offset) / 20, 100);

    const int cumulative_offset = floor(_floor_count - 1).x;
    const int base_amplitude = _floor_count / 2 + abs_value(cumulative_offset) / 20;
    _tower_sway_amplitude = min_value(base_amplitude, (_floor_count * base_amplitude) / 6);
    _tower_global_x = (-_tower_sway_wave * _tower_sway_amplitude) / 10000;

    int delta_x = _tower_global_x;
    int delta_y = 0;
    int running_angle = 0;
    const int settle_elapsed = _clock_ms - _top_settle_start_ms;

    for(int index = first_visible; index < _floor_count; ++index)
    {
        const int offset = floor(index).offset;
        if(index == _floor_count - 1 && settle_elapsed >= 0)
        {
            if(settle_elapsed < 100)
            {
                running_angle = running_angle / 8 + offset / 6 + (settle_elapsed * offset) / 600;
            }
            else if(settle_elapsed < 500)
            {
                running_angle = running_angle / 8 + offset / 6 + ((500 - settle_elapsed) * offset) / 2400;
                _top_settle_cached_angle = running_angle;
            }
            else if(settle_elapsed < 800)
            {
                running_angle = _top_settle_cached_angle -
                        ((_top_settle_cached_angle - running_angle) * (settle_elapsed - 500)) / 300;
            }
        }

        int sway_correction = 0;
        if(_tower_sway_wave > 0)
        {
            if(offset < 0)
            {
                sway_correction = (_tower_instability * offset * _tower_sway_wave) / 29491200;
            }
            else
            {
                sway_correction = (-_tower_instability * offset * _tower_sway_wave) / 58982400;
            }
        }
        else if(offset > 0)
        {
            sway_correction = (-_tower_instability * offset * _tower_sway_wave) / 29491200;
        }
        else
        {
            sway_correction = (_tower_instability * offset * _tower_sway_wave) / 58982400;
        }

        running_angle += sway_correction;
        delta_x += 2 * running_angle;
        const int half_angle = java_shift_right_one(running_angle);
        delta_y += _tower_sway_wave > 0 ? half_angle : -half_angle;

        TowerConstructionRenderPose& pose = _floor_render_poses[index % stored_floor_count];
        pose.x_delta = delta_x;
        pose.y_delta = delta_y;
        pose.z_angle_degrees = index == 0 ? 0 : -running_angle;
    }
}

int TowerConstruction::_next_visual_random(int bound)
{
    assert(bound > 0);
    _visual_random_state = (_visual_random_state * java_random_multiplier + java_random_addend) & java_random_mask;
    const uint32_t raw = uint32_t(_visual_random_state >> 16);
    const int32_t signed_raw = int32_t(raw);
    const int remainder = signed_raw % bound;
    return remainder < 0 ? -remainder : remainder;
}

void TowerConstruction::_update_crane(int delta_ms)
{
    _swing_phase_ms += delta_ms;

    if(_block_state == TowerConstructionBlockState::Raising)
    {
        _rope_length += (2 * delta_ms) / 3;
        if(_rope_length >= max_rope_length)
        {
            _rope_length = max_rope_length;
            _block_state = TowerConstructionBlockState::Attached;
        }
    }

    const int angle = ((200 * _swing_phase_ms) / _swing_period_ms) % 360;
    _crane_x = (_swing_amplitude_x * java_cos(angle)) >> 15;
    const int vertical_component = -((_swing_amplitude_y * java_sin(angle)) >> 15);
    _crane_y = _world_anchor_y - _vertical_swing_bias - _rope_length + vertical_component;

    if(_block_state == TowerConstructionBlockState::Attached)
    {
        _velocity_x = ((_crane_x - _previous_crane_x) * 256) / delta_ms;
        _velocity_y = ((_crane_y - _previous_crane_y) * 256) / delta_ms;
    }

    _previous_crane_x = _crane_x;
    _previous_crane_y = _crane_y;
}

void TowerConstruction::_update_falling(int delta_ms)
{
    const int elapsed_ms = _clock_ms - _drop_start_ms;
    _current_x += (_drop_velocity_x * delta_ms) / 512;
    _current_y = _drop_start_y + (_drop_velocity_y * elapsed_ms) / 256 - (elapsed_ms * elapsed_ms) / 200;

    if(_block_state == TowerConstructionBlockState::Slipping)
    {
        if(_current_z_angle_degrees < _slip_target_z_angle_degrees)
        {
            _current_z_angle_degrees = min_value(
                    _slip_target_z_angle_degrees,
                    _current_z_angle_degrees +
                            (elapsed_ms * (_slip_target_z_angle_degrees - _current_z_angle_degrees)) / 500);
        }
        else if(_current_z_angle_degrees > _slip_target_z_angle_degrees)
        {
            _current_z_angle_degrees = max_value(
                    _slip_target_z_angle_degrees,
                    _current_z_angle_degrees -
                            (elapsed_ms * (_current_z_angle_degrees - _slip_target_z_angle_degrees)) / 500);
        }
    }

    if(_current_y < _camera_y - view_half_fixed)
    {
        _register_miss();
        return;
    }

    if(_block_state == TowerConstructionBlockState::Falling)
    {
        _check_collision();
    }
}

void TowerConstruction::_check_collision()
{
    if(_floor_count == 0)
    {
        if(_current_y > -256 && _current_y < 256 && _current_x > -256 && _current_x < 256)
        {
            _resolve_landing(-1, 0);
        }
        return;
    }

    const int first_index = max_value(0, _floor_count - 5);
    for(int index = _floor_count - 1; index >= first_index; --index)
    {
        const TowerConstructionFloor& candidate = floor(index);
        if(_current_y > candidate.y - 256 && _current_y < candidate.y + 256 &&
           _current_x > candidate.x - 256 && _current_x < candidate.x + 256)
        {
            if(index == _floor_count - 1)
            {
                _resolve_landing(index, candidate.x);
            }
            else
            {
                _begin_slip(_current_x - candidate.x);
            }
            return;
        }
    }
}

void TowerConstruction::_resolve_landing(int, int floor_x)
{
    const int offset = _current_x - floor_x;
    if(abs_value(offset) > landing_max_abs_offset)
    {
        _begin_slip(offset);
        return;
    }

    _last_accuracy = accuracy_for_offset(offset);

    if(_roof_phase)
    {
        const int quality = 128 - abs_value(offset);
        const int bonus = _trophy_eligible ? (quality * _building_type) / 2 : quality / (5 - _building_type);
        _population += bonus;
        _last_population_award = bonus;
        _roof_result = _trophy_eligible ? 2 : 1;
        _combo_meter_ms = 0;
        _add_floor(offset, true);
        _enter_terminal();
        return;
    }

    if(_combo_count == 0)
    {
        _combo_bonus_pending = 0;
        ++_combo_count;
    }
    else if(_combo_meter_ms > 0)
    {
        ++_combo_count;
    }

    if(_last_accuracy == TowerConstructionAccuracyBand::Perfect)
    {
        _combo_meter_ms = 6000;
    }

    _award_population(int(_last_accuracy));
    _add_floor(offset, false);
    _enter_roof_phase_if_ready();
}

void TowerConstruction::_begin_slip(int offset)
{
    const int direction = offset < 0 ? -1 : 1;
    _block_state = TowerConstructionBlockState::Slipping;
    _last_accuracy = TowerConstructionAccuracyBand::None;
    _drop_start_x = _current_x;
    _drop_start_y = _current_y;
    _drop_start_ms = _clock_ms;
    _drop_velocity_x = direction * 500;
    _drop_velocity_y = 50;
    _current_z_angle_degrees = 0;
    _slip_target_z_angle_degrees = -direction * 45;
}

void TowerConstruction::_register_miss()
{
    if(_block_state == TowerConstructionBlockState::Missed || _status != TowerConstructionStatus::Playing)
    {
        return;
    }

    if(_combo_meter_ms > 0)
    {
        _settle_combo();
    }

    if(_chances_left > 0)
    {
        --_chances_left;
    }

    _last_accuracy = TowerConstructionAccuracyBand::None;
    _block_state = TowerConstructionBlockState::Missed;
    _camera_impact_start_ms = _clock_ms;
    _transition_start_ms = _clock_ms;

    if(_chances_left == 0)
    {
        _roof_result = 0;
        _enter_terminal();
    }
}

void TowerConstruction::_award_population(int accuracy_points)
{
    if(accuracy_points <= 0)
    {
        return;
    }

    if(_combo_meter_ms > 0)
    {
        _combo_bonus_pending += _combo_count * (2 + 2 * (_floor_count / 10));
    }

    const int award = (_floor_count / 10) + accuracy_points;
    _population += award;
    _last_population_award = award;
}

void TowerConstruction::_settle_combo()
{
    _population += _combo_bonus_pending;
    _last_population_award = _combo_bonus_pending;
    _combo_count = 0;
    _combo_meter_ms = 0;
}

void TowerConstruction::_update_combo(int delta_ms)
{
    if(_combo_meter_ms > 0)
    {
        _combo_meter_ms -= delta_ms + ((_combo_count - 1) * delta_ms) / 6;
        if(_combo_meter_ms <= 0)
        {
            _settle_combo();
        }
    }
    else if(_combo_meter_ms > -2000)
    {
        _combo_meter_ms -= delta_ms;
    }
}

void TowerConstruction::_add_floor(int offset, bool roof)
{
    TowerConstructionFloor floor_record;
    if(_floor_count == 0)
    {
        floor_record.x = offset;
        floor_record.y = first_floor_center_y;
    }
    else
    {
        const TowerConstructionFloor& previous = floor(_floor_count - 1);
        floor_record.x = previous.x + offset;
        floor_record.y = previous.y + fixed_floor_height;
    }
    floor_record.offset = offset;
    floor_record.roof = roof;
    _floors[_floor_count % stored_floor_count] = floor_record;
    ++_floor_count;
    _top_settle_start_ms = _clock_ms;
    _top_settle_cached_angle = 0;

    _current_x = floor_record.x;
    _current_y = floor_record.y;
    _block_state = TowerConstructionBlockState::Settled;
    _transition_start_ms = _clock_ms;

    if(! roof)
    {
        _update_difficulty();
    }

    _camera_transition_start_ms = _clock_ms;
    if(_floor_count > 1)
    {
        _camera_target_y += fixed_floor_height;
    }
    else
    {
        _camera_target_y = 512;
    }

    _update_tower_poses();
}

void TowerConstruction::_enter_roof_phase_if_ready()
{
    if(_floor_count != _target_height - 1)
    {
        return;
    }

    if(_combo_meter_ms > 0)
    {
        _settle_combo();
    }
    _roof_phase = true;
    if(_trophy_eligible && _population < trophy_population_table[_building_type - 1])
    {
        _trophy_eligible = false;
    }
}

void TowerConstruction::_update_difficulty()
{
    const int family = _building_type;
    _swing_period_ms = period_table[family] -
            (_floor_count * (period_table[family] - period_table[family + 2])) / _target_height;
    const int half_target = _target_height >> 1;
    _swing_amplitude_x = min_value(
            swing_x_table[family],
            swing_x_table[1] + (_floor_count * (swing_x_table[family] - swing_x_table[1])) / half_target);
    _swing_amplitude_y = min_value(
            swing_y_table[family],
            swing_y_table[1] + (_floor_count * (swing_y_table[family] - swing_y_table[1])) / half_target);
    _vertical_swing_bias = -min_value(128, (_floor_count * 256) / 100);
}

void TowerConstruction::_spawn_next_block()
{
    if(_status != TowerConstructionStatus::Playing)
    {
        return;
    }

    _rope_length = max_rope_length;
    _block_state = TowerConstructionBlockState::Attached;
    _current_x = _crane_x;
    _current_y = _crane_y;
    _drop_velocity_x = 0;
    _drop_velocity_y = 0;
    _current_z_angle_degrees = 0;
    _slip_target_z_angle_degrees = 0;
}

void TowerConstruction::_enter_terminal()
{
    if(_status != TowerConstructionStatus::Terminal)
    {
        _status = TowerConstructionStatus::Terminal;
        _transition_start_ms = _clock_ms;
    }
}
}
