#include <iostream>

#include "tb/tower_construction.h"

namespace
{
tb::InputFrame fresh_a()
{
    return tb::InputFrame{tb::key_mask(tb::Key::A), tb::key_mask(tb::Key::A)};
}

void emit(const char* label, const tb::TowerConstruction& construction, int tick_index)
{
    const tb::TowerConstructionSnapshot snapshot = construction.snapshot();
    std::cout << label << ',' << tick_index << ',' << int(snapshot.status) << ',' << int(snapshot.block_state) << ','
              << int(snapshot.building_type) << ',' << snapshot.target_height << ',' << snapshot.floor_count << ','
              << snapshot.chances_left << ',' << snapshot.population << ',' << int(snapshot.roof_phase) << ','
              << int(snapshot.trophy_eligible) << ',' << int(snapshot.roof_result) << ',' << snapshot.camera_y << ','
              << snapshot.camera_target_y << ',' << snapshot.presentation_camera_y << ','
              << int(snapshot.camera_impact_active) << ',' << snapshot.current_x << ',' << snapshot.current_y << ','
              << snapshot.rope_length << ',' << snapshot.swing_period_ms << ',' << snapshot.swing_amplitude_x << ','
              << snapshot.swing_amplitude_y << ',' << snapshot.vertical_swing_bias << ',' << snapshot.drop_velocity_x
              << ',' << snapshot.drop_velocity_y << ',' << int(snapshot.last_accuracy) << ',' << snapshot.combo_count
              << ',' << snapshot.combo_bonus_pending << ',' << snapshot.combo_meter_ms << ','
              << snapshot.current_z_angle_degrees;

    const int first_visible = construction.floor_count() > 5 ? construction.floor_count() - 5 : 0;
    for(int index = first_visible; index < construction.floor_count(); ++index)
    {
        const tb::TowerConstructionFloor& floor = construction.floor(index);
        const tb::TowerConstructionRenderPose& pose = construction.floor_render_pose(index);
        std::cout << ';' << floor.x << ':' << floor.y << ':' << floor.offset << ':' << int(floor.roof) << ':'
                  << pose.x_delta << ':' << pose.y_delta << ':' << pose.z_angle_degrees;
    }
    std::cout << '\n';
}

void tick(tb::TowerConstruction& construction, int& tick_index, const char* label, const tb::InputFrame& input = {})
{
    construction.update(25, input);
    ++tick_index;
    emit(label, construction, tick_index);
}

bool advance_until_ready(tb::TowerConstruction& construction, int& tick_index, const char* label)
{
    for(int guard = 0; guard < 500; ++guard)
    {
        const tb::TowerConstructionSnapshot snapshot = construction.snapshot();
        if(snapshot.status != tb::TowerConstructionStatus::Playing)
        {
            return false;
        }
        if(snapshot.block_state == tb::TowerConstructionBlockState::Attached &&
           snapshot.camera_y == snapshot.camera_target_y)
        {
            return true;
        }
        tick(construction, tick_index, label);
    }
    return false;
}

bool land_next_floor(tb::TowerConstruction& construction, int& tick_index, int wait_ticks, const char* label)
{
    if(! advance_until_ready(construction, tick_index, label))
    {
        return false;
    }

    const int floor_before = construction.floor_count();
    const int chances_before = construction.snapshot().chances_left;
    for(int wait = 0; wait < wait_ticks; ++wait)
    {
        tick(construction, tick_index, label);
    }
    tick(construction, tick_index, label, fresh_a());

    for(int guard = 0; guard < 500; ++guard)
    {
        const tb::TowerConstructionSnapshot snapshot = construction.snapshot();
        if(snapshot.floor_count > floor_before)
        {
            return true;
        }
        if(snapshot.chances_left < chances_before)
        {
            return false;
        }
        tick(construction, tick_index, label);
    }
    return false;
}
}

int main()
{
    constexpr int completion_waits[] = {23, 33, 26, 28, 25, 25, 24, 23, 24, 20};
    tb::TowerConstruction construction;
    construction.start(1, 10, true);
    int tick_index = 0;
    emit("MAIN", construction, tick_index);

    for(int goal = 1; goal <= 10; ++goal)
    {
        if(! land_next_floor(construction, tick_index, completion_waits[goal - 1], "MAIN"))
        {
            std::cerr << "failed to land construction floor " << goal << '\n';
            return 2;
        }
    }

    for(int guard = 0; guard < 100 && construction.snapshot().status != tb::TowerConstructionStatus::Results; ++guard)
    {
        tick(construction, tick_index, "MAIN");
    }

    const tb::TowerConstructionResult result = construction.result();
    if(! result.ready)
    {
        std::cerr << "construction result never became ready\n";
        return 3;
    }

    constexpr int ordinary_waits[] = {23, 33, 26, 28, 25, 25, 24, 23, 24};
    tb::TowerConstruction retry;
    retry.start(1, 10, false);
    int retry_tick = 0;
    emit("RETRY", retry, retry_tick);

    for(int goal = 1; goal <= 9; ++goal)
    {
        if(! land_next_floor(retry, retry_tick, ordinary_waits[goal - 1], "RETRY"))
        {
            std::cerr << "failed to land retry-path floor " << goal << '\n';
            return 4;
        }
    }

    if(! advance_until_ready(retry, retry_tick, "RETRY"))
    {
        return 5;
    }
    const int chances_before = retry.snapshot().chances_left;
    tick(retry, retry_tick, "RETRY", fresh_a());
    for(int guard = 0; guard < 500 && retry.snapshot().chances_left == chances_before; ++guard)
    {
        tick(retry, retry_tick, "RETRY");
    }
    if(retry.snapshot().chances_left != chances_before - 1 || ! retry.snapshot().roof_phase)
    {
        std::cerr << "roof miss did not consume exactly one retry\n";
        return 6;
    }
    std::cout << "ROOF_RETRY " << retry.snapshot().floor_count << ' ' << retry.snapshot().chances_left << '\n';

    if(! land_next_floor(retry, retry_tick, 33, "RETRY"))
    {
        std::cerr << "roof retry did not land\n";
        return 7;
    }
    for(int guard = 0; guard < 100 && retry.snapshot().status != tb::TowerConstructionStatus::Results; ++guard)
    {
        tick(retry, retry_tick, "RETRY");
    }
    const tb::TowerConstructionResult retry_result = retry.result();
    if(! retry_result.ready || retry_result.roof != 1 || retry.snapshot().chances_left != 2)
    {
        std::cerr << "roof retry result mismatch\n";
        return 8;
    }

    std::cout << "RETRY_RESULT " << int(retry_result.building_type) << ' ' << retry_result.population << ' '
              << int(retry_result.roof) << ' ' << retry.snapshot().floor_count << ' '
              << retry.snapshot().chances_left << '\n';
    std::cout << "RESULT " << int(result.building_type) << ' ' << result.population << ' ' << int(result.roof) << ' '
              << construction.snapshot().floor_count << ' ' << construction.snapshot().chances_left << '\n';
    return 0;
}
