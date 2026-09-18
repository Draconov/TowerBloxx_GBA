#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>

#include "tb/app_state.h"
#include "tb/build_city.h"
#include "tb/build_city_events.h"
#include "tb/crane_presentation.h"
#include "tb/hall_of_fame.h"
#include "tb/menu_clouds.h"
#include "tb/quick_game.h"
#include "tb/save_data.h"
#include "tb/tower_construction.h"
#include "tb/tower_session.h"
#include "tb/ui_controller.h"

namespace
{
tb::InputFrame no_input()
{
    return {};
}

tb::InputFrame fresh(tb::Key key)
{
    return tb::InputFrame{tb::key_mask(key), tb::key_mask(key)};
}

void test_input_and_ui_shell_flow()
{
    tb::AppState app;
    assert(app.scene() == tb::SceneId::UiShell);

    auto frame = app.update_input(tb::key_mask(tb::Key::Left));
    assert(frame.held(tb::Key::Left));
    assert(frame.pressed(tb::Key::Left));
    frame = app.update_input(tb::key_mask(tb::Key::Left));
    assert(! frame.pressed(tb::Key::Left));

    tb::SaveData save = tb::make_default_save();
    tb::UiController ui(save);
    assert(ui.scene() == tb::UiScene::Title);
    const auto result = ui.update(fresh(tb::Key::A), save);
    assert(result.action == tb::UiAction::None);
    assert(ui.scene() == tb::UiScene::MainMenu);
    assert(ui.root_menu_count() == 5);
    assert(ui.root_menu_item(0) == tb::RootMenuItem::BuildCity);
    assert(ui.root_menu_item(1) == tb::RootMenuItem::QuickGame);
}

void test_save_and_records()
{
    tb::SaveData save = tb::make_default_save();
    assert(save.magic == tb::save_magic);
    assert(save.version == tb::save_version);
    assert(tb::valid_save(save));

    save.city_tiles[12].type = 3;
    save.city_tiles[12].population = 987;
    save.city_tiles[12].roof = 2;
    assert(! tb::valid_save(save));
    tb::finalize_save(save);
    assert(tb::valid_save(save));

    tb::LegacySaveDataV1 legacy{};
    legacy.language = 3;
    legacy.sound_enabled = 0;
    legacy.quick_high_score = 777;
    tb::finalize_legacy_save_for_test(legacy);
    const tb::SaveData migrated = tb::migrate_legacy_save(legacy);
    assert(tb::valid_save(migrated));
    assert(migrated.language == 3);
    assert(migrated.quick_best_population == 777);

    tb::SaveData records = tb::make_default_save();
    const tb::QuickRecordFlags flags = tb::apply_quick_result(records, tb::QuickGameResult{50, 12, 4});
    assert(flags.population && flags.height && flags.combo);
    assert(records.quick_best_population == 50);
    assert(records.quick_best_height == 12);
    assert(records.quick_best_combo == 4);
}

void test_first_block_intro_and_release_gate()
{
    tb::TowerConstruction construction;
    construction.start(1, 10, false);
    auto snapshot = construction.snapshot();
    assert(snapshot.status == tb::TowerConstructionStatus::Playing);
    assert(snapshot.block_state == tb::TowerConstructionBlockState::Attached);
    assert(snapshot.rope_length == 1664);
    assert(snapshot.camera_y == 2432);
    assert(snapshot.camera_target_y == 512);

    construction.update(25, fresh(tb::Key::A));
    snapshot = construction.snapshot();
    assert(snapshot.block_state == tb::TowerConstructionBlockState::Attached);
    assert(snapshot.camera_y < 2432);
    assert(snapshot.camera_y > 512);

    int elapsed_ms = 25;
    while(construction.snapshot().camera_y != construction.snapshot().camera_target_y && elapsed_ms < 5000)
    {
        construction.update(25, no_input());
        elapsed_ms += 25;
    }

    snapshot = construction.snapshot();
    assert(snapshot.camera_y == 512);
    assert(snapshot.rope_length == 1664);
    assert(elapsed_ms >= 3400 && elapsed_ms <= 3600);

    construction.update(25, fresh(tb::Key::A));
    assert(construction.snapshot().block_state == tb::TowerConstructionBlockState::Falling);

    tb::QuickGame quick;
    auto quick_snapshot = quick.snapshot();
    assert(quick_snapshot.block_state == tb::QuickBlockState::Attached);
    assert(quick_snapshot.rope_length == 1664);
    assert(quick_snapshot.camera_y == 2432);
    quick.update(25, fresh(tb::Key::A));
    assert(quick.snapshot().block_state == tb::QuickBlockState::Attached);
}


void test_special_crane_screen_anchor()
{
    // The first JAR camera reveal starts with the crane world anchor at 2432
    // and camera at 2432, then moves the camera toward 512 while the anchor
    // stays fixed. Once gameplay settles, the source anchor is -165 px.
    assert(tb::special_crane_cable_start_y(2432, true) == 0);
    assert(tb::special_crane_cable_start_y(512, true) == -165);
    assert(tb::special_crane_cable_start_y(512, false) == -165);
    assert(tb::special_crane_boom_part_center_x(0, 0) == -115);
    assert(tb::special_crane_boom_part_center_x(0, 1) == -51);
    assert(tb::special_crane_boom_part_center_x(0, 2) == -3);
    assert(tb::special_crane_boom_center_y(0) == -15);
}

void test_perfect_landing_feedback_geometry()
{
    assert(tb::perfect_landing_star_count == 4);
    assert(tb::perfect_landing_star_duration_ms == 560);
    assert(tb::perfect_landing_star_trail_delay_ms == 14);
    assert(tb::perfect_landing_trail_sample_count == 7);
    assert(tb::perfect_landing_sprites_per_star == 8);
    assert(tb::perfect_landing_sprite_capacity == 32);

    for(int index = 0; index < tb::perfect_landing_star_count; ++index)
    {
        assert(tb::perfect_landing_star_offset_x(index, 0) == 0);
        assert(tb::perfect_landing_star_offset_y(index, 0) == 0);
    }

    // The burst originates at the landed block centre and spreads wider than
    // the previous compact pass: the outer stars reach at least +/- 32 px.
    assert(tb::perfect_landing_star_offset_x(0, tb::perfect_landing_star_duration_ms) <= -46);
    assert(tb::perfect_landing_star_offset_x(3, tb::perfect_landing_star_duration_ms) >= 46);
    assert(tb::perfect_landing_star_offset_y(2, tb::perfect_landing_star_duration_ms) <= -32);
    assert(tb::perfect_landing_star_offset_y(1, tb::perfect_landing_star_duration_ms) >= 32);
    assert(tb::perfect_landing_pattern_bucket(0) == 0);
    assert(tb::perfect_landing_pattern_bucket(17) == 5);
    assert(tb::perfect_landing_star_offset_x(0, tb::perfect_landing_star_duration_ms, 7) <
           tb::perfect_landing_star_offset_x(0, tb::perfect_landing_star_duration_ms));
    assert(tb::perfect_landing_star_offset_y(3, tb::perfect_landing_star_duration_ms, 7) !=
           tb::perfect_landing_star_offset_y(3, tb::perfect_landing_star_duration_ms));
    assert(tb::perfect_landing_star_offset_x(2, tb::perfect_landing_star_duration_ms, 3) !=
           tb::perfect_landing_star_offset_x(2, tb::perfect_landing_star_duration_ms, 8));
    assert(tb::perfect_landing_star_trail_elapsed(10, 0) == 0);
    assert(tb::perfect_landing_star_trail_elapsed(40, 0) == 26);
    assert(tb::perfect_landing_star_trail_elapsed(20, 1) == 0);
    assert(tb::perfect_landing_star_trail_elapsed(28, 1) == 0);
    assert(tb::perfect_landing_star_trail_elapsed(80, 1) == 52);

    assert(tb::perfect_landing_seam_phase(0) == tb::PerfectLandingSeamPhase::White);
    assert(tb::perfect_landing_seam_phase(49) == tb::PerfectLandingSeamPhase::White);
    assert(tb::perfect_landing_seam_phase(50) == tb::PerfectLandingSeamPhase::Yellow);
    assert(tb::perfect_landing_seam_phase(129) == tb::PerfectLandingSeamPhase::Yellow);
    assert(tb::perfect_landing_seam_phase(130) == tb::PerfectLandingSeamPhase::Hidden);
}

void test_roof_phase_uses_stationary_camera_lowering()
{
    tb::TowerConstruction construction;
    construction.start(1, 10, true);
    for(int floor = 0; floor < 9; ++floor)
    {
        construction.debug_resolve_landing_for_test(0);
    }

    auto snapshot = construction.snapshot();
    assert(snapshot.roof_phase);
    const int target_camera_y = snapshot.camera_target_y;

    // Roof presentation must wait for the last camera move to finish before
    // the special crane starts lowering the roof from a zero-length rope.
    for(int elapsed = 0; elapsed < 1200 &&
            construction.snapshot().block_state == tb::TowerConstructionBlockState::Settled; elapsed += 25)
    {
        construction.update(25, no_input());
    }
    snapshot = construction.snapshot();
    assert(snapshot.camera_y == target_camera_y);
    assert(snapshot.block_state == tb::TowerConstructionBlockState::Raising);
    assert(snapshot.rope_length < 128);

    const int fixed_camera_y = snapshot.camera_y;
    const int initial_rope = snapshot.rope_length;
    construction.update(500, no_input());
    snapshot = construction.snapshot();
    assert(snapshot.camera_y == fixed_camera_y);
    assert(snapshot.block_state == tb::TowerConstructionBlockState::Raising);
    assert(snapshot.rope_length > initial_rope);
    assert(snapshot.rope_length < 1664);

    for(int elapsed = 0; elapsed < 3500 &&
            construction.snapshot().block_state == tb::TowerConstructionBlockState::Raising; elapsed += 25)
    {
        construction.update(25, no_input());
    }
    snapshot = construction.snapshot();
    assert(snapshot.camera_y == fixed_camera_y);
    assert(snapshot.block_state == tb::TowerConstructionBlockState::Attached);
    assert(snapshot.rope_length == 1664);
}


void test_tower_combo_roof_and_failure_rules()
{
    tb::TowerConstruction construction;
    construction.start(1, 10, true);
    for(int floor = 0; floor < 9; ++floor)
    {
        construction.debug_resolve_landing_for_test(0);
    }
    auto snapshot = construction.snapshot();
    assert(snapshot.floor_count == 9);
    assert(snapshot.population == 126);
    assert(snapshot.roof_phase);
    assert(snapshot.trophy_eligible);

    construction.debug_register_miss_for_test();
    snapshot = construction.snapshot();
    assert(snapshot.chances_left == 2);
    assert(snapshot.roof_phase);
    construction.debug_resolve_landing_for_test(0);
    assert(construction.snapshot().roof_result == 2);

    tb::TowerConstruction failed;
    failed.start(2, 20, false);
    failed.debug_register_miss_for_test();
    failed.debug_register_miss_for_test();
    failed.debug_register_miss_for_test();
    assert(failed.snapshot().chances_left == 0);
    assert(failed.snapshot().status == tb::TowerConstructionStatus::Terminal);
}

void test_build_city_progress_and_replacement()
{
    constexpr std::array<int, 5> populations = {0, 250, 2200, 8000, 19000};
    constexpr std::array<int, 5> milestones = {0, 3, 10, 15, 20};
    constexpr std::array<int, 5> levels = {0, 1, 4, 7, 9};
    constexpr std::array<int, 5> unlocked = {1, 2, 4, 4, 4};

    for(int index = 0; index < int(populations.size()); ++index)
    {
        tb::SaveData save = tb::make_default_save();
        if(populations[index] > 0)
        {
            save.city_tiles[0] = {populations[index], 1, 1, 0, 0};
        }
        tb::BuildCity city(save);
        const auto snapshot = city.snapshot();
        assert(snapshot.milestone == milestones[index]);
        assert(snapshot.city_level == levels[index]);
        assert(snapshot.max_unlocked_building_type == unlocked[index]);
    }

    tb::SaveData save = tb::make_default_save();
    save.city_tiles[12] = {1000, 1, 1, 0, 0};
    save.city_tiles[11] = {100, 1, 1, 0, 0};
    save.city_tiles[13] = {100, 2, 1, 0, 0};
    save.city_tiles[7] = {100, 3, 1, 0, 0};
    tb::BuildCity city(save);
    const int before = city.snapshot().total_population;
    city.accept_constructed_tower(4, 444, 2);
    city.update(750, {}, save);
    city.update(16, fresh(tb::Key::A), save);
    const auto committed = city.update(3001, {}, save);
    assert(committed.save_dirty && committed.placement_committed);
    assert(save.city_tiles[12].type == 4);
    assert(save.city_tiles[12].population == 444);
    assert(city.snapshot().total_population == before - 1000 + 444);
}

void test_build_city_events_and_hall_of_fame()
{
    tb::SaveData save = tb::make_default_save();
    tb::BuildCityEventController events;
    tb::BuildCityProgressState fresh_city{};
    events.on_city_entered(fresh_city, save);
    assert(events.has_event());
    assert(events.current_event().id == 0);
    assert(events.pending_count() == 3);
    while(events.has_event())
    {
        assert(events.acknowledge(save));
    }

    tb::HallOfFameData hall{};
    tb::reset_hall_of_fame(hall);
    auto q = tb::insert_hall_score(hall, tb::HallTable::QuickGame, 100, "mike!");
    assert(q.qualifies && q.position == 1);
    assert(std::strcmp(hall.tables[1][0].name.data(), "MIKE") == 0);

    q = tb::insert_build_city_player_score(hall, 5000, "city-01.");
    assert(q.qualifies);
    assert(tb::build_city_player_registered(hall));
    assert(tb::update_build_city_player_score(hall, 7000));
    assert(hall.tables[0][0].score == 7000);
}


void test_menu_cloud_field_matches_reference_motion()
{
    tb::MenuCloudField clouds;
    clouds.reset();
    assert(tb::MenuCloudField::cloud_count == 8);

    bool saw_small = false;
    bool saw_large = false;
    for(int index = 0; index < tb::MenuCloudField::cloud_count; ++index)
    {
        const auto& cloud = clouds.cloud(index);
        assert(cloud.type == 0 || cloud.type == 1);
        assert(cloud.vx == 0);
        assert(cloud.vy == (cloud.type == 0 ? 6 : 9));
        saw_small |= cloud.type == 0;
        saw_large |= cloud.type == 1;
    }
    assert(saw_small && saw_large);

    const int before_y = clouds.cloud(0).y_fixed;
    assert(clouds.update(25));
    assert(clouds.cloud(0).y_fixed > before_y);
}

void test_session_suspend_resume()
{
    tb::TowerSessionCoordinator session;
    assert(session.foreground() == tb::RuntimeScene::Ui);
    session.start_quick_game();
    assert(session.foreground() == tb::RuntimeScene::QuickGame);
    session.suspend_quick_game();
    assert(session.suspended_kind() == tb::SuspendedSessionKind::QuickGame);
    assert(session.resume_suspended() == tb::SuspendedSessionKind::QuickGame);
    session.start_build_city();
    session.start_construction();
    session.suspend_construction();
    assert(session.suspended_kind() == tb::SuspendedSessionKind::BuildCityConstruction);
}
}

int main()
{
    test_input_and_ui_shell_flow();
    test_save_and_records();
    test_first_block_intro_and_release_gate();
    test_special_crane_screen_anchor();
    test_perfect_landing_feedback_geometry();
    test_roof_phase_uses_stationary_camera_lowering();
    test_tower_combo_roof_and_failure_rules();
    test_build_city_progress_and_replacement();
    test_build_city_events_and_hall_of_fame();
    test_menu_cloud_field_matches_reference_motion();
    test_session_suspend_resume();
    std::cout << "towerbloxx permanent gameplay regressions ok\n";
    return 0;
}
