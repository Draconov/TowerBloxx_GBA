#include <cassert>
#include <iostream>

#include "tb/tower_construction.h"

namespace
{
tb::InputFrame no_input()
{
    return tb::InputFrame{};
}

void advance_to_results(tb::TowerConstruction& construction)
{
    for(int index = 0; index < 16 && construction.snapshot().status != tb::TowerConstructionStatus::Results; ++index)
    {
        construction.update(150, no_input());
    }
}
}

int main()
{
    tb::TowerConstruction construction;
    construction.start(1, 10, true);
    auto snapshot = construction.snapshot();
    assert(snapshot.status == tb::TowerConstructionStatus::Playing);
    assert(snapshot.building_type == 1);
    assert(snapshot.target_height == 10);
    assert(snapshot.chances_left == 3);
    assert(snapshot.floor_count == 0);
    assert(snapshot.population == 0);
    assert(! snapshot.roof_phase);
    assert(snapshot.trophy_requested);
    assert(snapshot.trophy_eligible);
    assert(snapshot.swing_period_ms == 1700);
    assert(snapshot.swing_amplitude_x == 128);
    assert(snapshot.swing_amplitude_y == 64);
    construction.update(25, no_input());
    snapshot = construction.snapshot();
    assert(snapshot.crane_x == snapshot.current_x);
    assert(snapshot.crane_y == snapshot.current_y);
    assert(snapshot.crane_angle_degrees == ((snapshot.crane_x >> 4) * 2) / 3);

    // The hook/crane keeps following the swing after release; only the block falls.
    for(int guard = 0; guard < 200 && construction.snapshot().block_state != tb::TowerConstructionBlockState::Attached; ++guard)
    {
        construction.update(25, no_input());
    }
    assert(construction.snapshot().block_state == tb::TowerConstructionBlockState::Attached);
    assert(construction.snapshot().current_z_angle_degrees == (construction.snapshot().crane_x >> 4));
    tb::InputFrame release{};
    release.pressed_mask = uint16_t(tb::Key::A);
    construction.update(25, release);
    snapshot = construction.snapshot();
    assert(snapshot.block_state == tb::TowerConstructionBlockState::Falling);
    const int released_block_y = snapshot.current_y;
    const int released_crane_y = snapshot.crane_y;
    construction.update(100, no_input());
    snapshot = construction.snapshot();
    assert(snapshot.current_y != released_block_y);
    assert(snapshot.crane_y != snapshot.current_y);
    assert(snapshot.crane_y != released_crane_y || snapshot.crane_x != snapshot.current_x);

    // Shared House slip presentation: +/-45 degree Z plus an independent
    // random +/-60 degree Y-axis tumble, both eased across 500 ms.
    tb::TowerConstruction slipping;
    slipping.start(1, 10, false);
    slipping.debug_resolve_landing_for_test(128);
    assert(slipping.snapshot().block_state == tb::TowerConstructionBlockState::Slipping);
    slipping.update(25, no_input());
    assert(slipping.snapshot().current_z_angle_degrees == -2);
    assert(slipping.snapshot().current_y_angle_degrees == 3 ||
           slipping.snapshot().current_y_angle_degrees == -3);

    construction.start(4, 40, true);
    snapshot = construction.snapshot();
    assert(snapshot.swing_period_ms == 1550);
    construction.debug_resolve_landing_for_test(0);
    snapshot = construction.snapshot();
    assert(snapshot.floor_count == 1);
    assert(snapshot.population == 4);
    assert(snapshot.swing_period_ms == 1548);
    assert(snapshot.swing_amplitude_x == 262);
    assert(snapshot.swing_amplitude_y == 109);
    assert(snapshot.vertical_swing_bias == -2);

    // Low-population Residential construction revokes the provisional trophy before the roof.
    construction.start(1, 10, true);
    for(int floor = 0; floor < 9; ++floor)
    {
        construction.debug_resolve_landing_for_test(100); // OK: one population on floors 0..8.
    }
    snapshot = construction.snapshot();
    assert(snapshot.floor_count == 9);
    assert(snapshot.population == 9);
    assert(snapshot.roof_phase);
    assert(snapshot.trophy_requested);
    assert(! snapshot.trophy_eligible);
    construction.debug_resolve_landing_for_test(0);
    snapshot = construction.snapshot();
    assert(snapshot.floor_count == 10);
    assert(snapshot.population == 41); // 9 + perfect normal-roof bonus 32.
    assert(snapshot.roof_result == 1);
    assert(snapshot.status == tb::TowerConstructionStatus::Terminal);
    advance_to_results(construction);
    assert(construction.snapshot().status == tb::TowerConstructionStatus::Results);
    auto result = construction.result();
    assert(result.ready);
    assert(result.building_type == 1);
    assert(result.population == 41);
    assert(result.roof == 1);

    // A high-population Residential tower keeps trophy eligibility and receives the trophy roof bonus.
    construction.start(1, 10, true);
    for(int floor = 0; floor < 9; ++floor)
    {
        construction.debug_resolve_landing_for_test(0);
    }
    snapshot = construction.snapshot();
    assert(snapshot.roof_phase);
    assert(snapshot.population == 126); // direct perfect awards + settled combo bonus.
    assert(snapshot.trophy_eligible);
    construction.debug_resolve_landing_for_test(0);
    snapshot = construction.snapshot();
    assert(snapshot.population == 190); // +64 perfect Residential trophy roof.
    assert(snapshot.roof_result == 2);

    // A roof miss consumes a retry but leaves the roof phase active; a later roof can still succeed.
    construction.start(1, 10, false);
    for(int floor = 0; floor < 9; ++floor)
    {
        construction.debug_resolve_landing_for_test(100);
    }
    assert(construction.snapshot().roof_phase);
    construction.debug_register_miss_for_test();
    snapshot = construction.snapshot();
    assert(snapshot.chances_left == 2);
    assert(snapshot.roof_phase);
    assert(snapshot.roof_result == 0);
    assert(snapshot.status == tb::TowerConstructionStatus::Playing);
    construction.debug_resolve_landing_for_test(0);
    assert(construction.snapshot().roof_result == 1);

    // Three misses anywhere cancel the construction and return roof result 0 after 2000 ms.
    construction.start(2, 20, true);
    construction.debug_register_miss_for_test();
    construction.debug_register_miss_for_test();
    construction.debug_register_miss_for_test();
    snapshot = construction.snapshot();
    assert(snapshot.chances_left == 0);
    assert(snapshot.status == tb::TowerConstructionStatus::Terminal);
    assert(snapshot.roof_result == 0);
    advance_to_results(construction);
    result = construction.result();
    assert(result.ready);
    assert(result.building_type == 2);
    assert(result.population == 0);
    assert(result.roof == 0);

    std::cout << "tower construction ok\n";
    return 0;
}
