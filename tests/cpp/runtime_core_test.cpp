#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>

#include "tb/app_state.h"
#include "tb/build_city.h"
#include "tb/save_data.h"
#include "tb/quick_game.h"
#include "tb/ui_controller.h"

namespace
{
tb::InputFrame fresh(tb::Key key)
{
    return tb::InputFrame{tb::key_mask(key), tb::key_mask(key)};
}
}

int main()
{
    tb::AppState app;
    assert(app.scene() == tb::SceneId::UiShell);

    auto frame = app.update_input(tb::key_mask(tb::Key::Left));
    assert(frame.held(tb::Key::Left));
    assert(frame.pressed(tb::Key::Left));

    frame = app.update_input(tb::key_mask(tb::Key::Left));
    assert(frame.held(tb::Key::Left));
    assert(! frame.pressed(tb::Key::Left));

    frame = app.update_input(0);
    assert(! frame.held(tb::Key::Left));
    assert(! frame.pressed(tb::Key::Left));

    frame = app.update_input(tb::key_mask(tb::Key::Left) | tb::key_mask(tb::Key::Start));
    assert(frame.pressed(tb::Key::Left));
    assert(frame.pressed(tb::Key::Start));

    tb::SaveData save = tb::make_default_save();
    assert(save.magic == tb::save_magic);
    assert(save.version == tb::save_version);
    assert(save.language == 0);
    assert(save.sound_enabled == 1);
    assert(tb::valid_save(save));

    assert(save.quick_best_population == 0);
    assert(save.quick_best_height == 0);
    assert(save.quick_best_combo == 0);
    for(const auto& tile : save.city_tiles)
    {
        assert(tile.type == 0);
        assert(tile.population == 0);
        assert(tile.roof == 0);
    }
    for(uint8_t flag : save.city_tutorial_flags)
    {
        assert(flag == 0);
    }
    assert(tb::save_version == 4);
    for(const auto& table : save.hall_of_fame.tables) for(const auto& entry : table) { assert(std::strcmp(entry.name.data(), "SUMEA") == 0); assert(entry.score == 0); }
    assert(std::strcmp(save.hall_of_fame.last_player_name.data(), "SUMEA") == 0);

    tb::SaveData city_round_trip = tb::make_default_save();
    city_round_trip.city_tiles[12].type = 3;
    city_round_trip.city_tiles[12].population = 987;
    city_round_trip.city_tiles[12].roof = 2;
    city_round_trip.city_tutorial_flags[45] = 1;
    assert(! tb::valid_save(city_round_trip));
    tb::finalize_save(city_round_trip);
    assert(tb::valid_save(city_round_trip));
    assert(city_round_trip.city_tiles[12].type == 3);
    assert(city_round_trip.city_tiles[12].population == 987);
    assert(city_round_trip.city_tiles[12].roof == 2);
    assert(city_round_trip.city_tutorial_flags[45] == 1);

    tb::LegacySaveDataV2 legacy_v2{};
    legacy_v2.language = 4;
    legacy_v2.sound_enabled = 0;
    legacy_v2.quick_best_population = 222;
    legacy_v2.quick_best_height = 33;
    legacy_v2.quick_best_combo = 7;
    tb::finalize_legacy_v2_save_for_test(legacy_v2);
    assert(tb::valid_legacy_v2_save(legacy_v2));
    const tb::SaveData migrated_v2 = tb::migrate_legacy_v2_save(legacy_v2);
    assert(migrated_v2.version == tb::save_version);
    assert(migrated_v2.language == 4);
    assert(migrated_v2.sound_enabled == 0);
    assert(migrated_v2.quick_best_population == 222);
    assert(migrated_v2.quick_best_height == 33);
    assert(migrated_v2.quick_best_combo == 7);
    assert(migrated_v2.city_tiles[12].type == 0);
    assert(migrated_v2.city_tiles[12].population == 0);
    assert(migrated_v2.city_tiles[12].roof == 0);
    assert(tb::valid_save(migrated_v2));
    tb::LegacySaveDataV3 legacy_v3{}; legacy_v3.language = 2; legacy_v3.sound_enabled = 0; legacy_v3.quick_best_population = 3333; legacy_v3.quick_best_height = 44; legacy_v3.quick_best_combo = 8; legacy_v3.city_tiles[0].type = 1; legacy_v3.city_tiles[0].population = 75; legacy_v3.city_tiles[0].roof = 1; legacy_v3.city_tiles[24].type = 4; legacy_v3.city_tiles[24].population = 2222; legacy_v3.city_tiles[24].roof = 2; legacy_v3.city_tutorial_flags[0] = 1; legacy_v3.city_tutorial_flags[45] = 1; tb::finalize_legacy_v3_save_for_test(legacy_v3); assert(tb::valid_legacy_v3_save(legacy_v3)); const tb::SaveData migrated_v3 = tb::migrate_legacy_v3_save(legacy_v3); assert(migrated_v3.version == tb::save_version); assert(migrated_v3.language == 2); assert(migrated_v3.sound_enabled == 0); assert(migrated_v3.quick_best_population == 3333); assert(migrated_v3.quick_best_height == 44); assert(migrated_v3.quick_best_combo == 8); assert(migrated_v3.city_tiles[0].population == 75); assert(migrated_v3.city_tiles[24].population == 2222); assert(migrated_v3.city_tutorial_flags[0] == 1); assert(migrated_v3.city_tutorial_flags[45] == 1); assert(std::strcmp(migrated_v3.hall_of_fame.tables[0][0].name.data(), "SUMEA") == 0); assert(tb::valid_save(migrated_v3));

    save.quick_best_population = 123456;
    assert(! tb::valid_save(save));
    tb::finalize_save(save);
    assert(tb::valid_save(save));

    tb::SaveData records = tb::make_default_save();
    const tb::QuickRecordFlags first_records = tb::apply_quick_result(records, tb::QuickGameResult{50, 12, 4});
    assert(first_records.population);
    assert(first_records.height);
    assert(first_records.combo);
    assert(records.quick_best_population == 50);
    assert(records.quick_best_height == 12);
    assert(records.quick_best_combo == 4);
    const tb::QuickRecordFlags equal_records = tb::apply_quick_result(records, tb::QuickGameResult{50, 12, 4});
    assert(! equal_records.population && ! equal_records.height && ! equal_records.combo);
    const tb::QuickRecordFlags mixed_records = tb::apply_quick_result(records, tb::QuickGameResult{49, 13, 5});
    assert(! mixed_records.population);
    assert(mixed_records.height);
    assert(mixed_records.combo);
    assert(records.quick_best_population == 50);
    assert(records.quick_best_height == 13);
    assert(records.quick_best_combo == 5);
    assert(! tb::valid_save(records));
    tb::finalize_save(records);
    assert(tb::valid_save(records));

    tb::LegacySaveDataV1 legacy{};
    legacy.language = 3;
    legacy.sound_enabled = 0;
    legacy.quick_high_score = 777;
    tb::finalize_legacy_save_for_test(legacy);
    assert(tb::valid_legacy_save(legacy));
    const tb::SaveData migrated = tb::migrate_legacy_save(legacy);
    assert(migrated.version == tb::save_version);
    assert(migrated.language == 3);
    assert(migrated.sound_enabled == 0);
    assert(migrated.quick_best_population == 777);
    assert(migrated.quick_best_height == 0);
    assert(migrated.quick_best_combo == 0);
    assert(tb::valid_save(migrated));

    tb::SaveData corrupt = save;
    corrupt.language ^= 1;
    assert(! tb::valid_save(corrupt));
    corrupt = save;
    corrupt.magic ^= 0x10;
    assert(! tb::valid_save(corrupt));
    corrupt = save;
    corrupt.version += 1;
    tb::finalize_save(corrupt);
    assert(! tb::valid_save(corrupt));

    // Reset City must match the original citymode reset boundary: all city tiles
    // and tutorial/event flags are cleared while settings and Quick Game records survive.
    save = tb::make_default_save();
    save.language = 3;
    save.sound_enabled = 0;
    save.quick_best_population = 4321;
    save.quick_best_height = 77;
    save.quick_best_combo = 9;
    save.city_tiles[0].type = 4;
    save.city_tiles[0].population = 999;
    save.city_tiles[0].roof = 2;
    save.city_tiles[24].type = 2;
    save.city_tiles[24].population = 123;
    save.city_tutorial_flags[0] = 1;
    save.city_tutorial_flags[45] = 1;
    assert(! tb::construction_instructions_seen(save));
    assert(tb::mark_construction_instructions_seen(save));
    assert(tb::construction_instructions_seen(save));
    assert(! tb::mark_construction_instructions_seen(save));
    const uint8_t construction_state_before_reset = save.reserved[0];
    tb::reset_city_progress(save);
    assert(save.language == 3);
    assert(save.sound_enabled == 0);
    assert(save.quick_best_population == 4321);
    assert(save.quick_best_height == 77);
    assert(save.quick_best_combo == 9);
    for(const tb::CityTileSave& tile : save.city_tiles)
    {
        assert(tile.type == 0);
        assert(tile.population == 0);
        assert(tile.roof == 0);
    }
    for(uint8_t flag : save.city_tutorial_flags)
    {
        assert(flag == 0);
    }
    assert(save.reserved[0] == construction_state_before_reset);
    assert(tb::construction_instructions_seen(save));

    save = tb::make_default_save();
    save = tb::make_default_save();
    tb::UiController ui(save);
    assert(ui.scene() == tb::UiScene::Title);
    assert(ui.selection() == 0);
    assert(ui.language() == 0);
    assert(ui.sound_enabled());

    auto result = ui.update(fresh(tb::Key::A), save);
    assert(! result.save_dirty);
    assert(result.action == tb::UiAction::None);
    assert(ui.scene() == tb::UiScene::MainMenu);
    assert(ui.root_menu_count() == 5);
    assert(ui.root_menu_item(0) == tb::RootMenuItem::BuildCity);
    assert(ui.root_menu_item(1) == tb::RootMenuItem::QuickGame);
    assert(ui.root_menu_item(2) == tb::RootMenuItem::HighScores);
    assert(ui.root_menu_item(3) == tb::RootMenuItem::Settings);
    assert(ui.root_menu_item(4) == tb::RootMenuItem::Instructions);

    // Root navigation wraps across the visible list and launches modes directly.
    ui.update(fresh(tb::Key::Up), save);
    assert(ui.selection() == 4);
    ui.update(fresh(tb::Key::Down), save);
    assert(ui.selection() == 0);
    result = ui.update(fresh(tb::Key::A), save);
    assert(result.action == tb::UiAction::StartBuildCity);
    assert(ui.scene() == tb::UiScene::MainMenu);

    tb::UiController quick_ui(save);
    quick_ui.update(fresh(tb::Key::A), save);
    quick_ui.update(fresh(tb::Key::Down), save);
    result = quick_ui.update(fresh(tb::Key::A), save);
    assert(result.action == tb::UiAction::StartQuickGame);

    // Continue is conditional and shifts the other root items down by one row.
    ui.set_suspended_session_available(true);
    assert(ui.root_menu_count() == 6);
    assert(ui.root_menu_item(0) == tb::RootMenuItem::ContinueGame);
    assert(ui.root_menu_item(1) == tb::RootMenuItem::BuildCity);
    assert(ui.root_menu_item(2) == tb::RootMenuItem::QuickGame);
    assert(ui.root_menu_item(3) == tb::RootMenuItem::HighScores);
    assert(ui.root_menu_item(4) == tb::RootMenuItem::Settings);
    assert(ui.root_menu_item(5) == tb::RootMenuItem::Instructions);

    // Continue emits a resume action.
    while(ui.selection() != 0) ui.update(fresh(tb::Key::Up), save);
    result = ui.update(fresh(tb::Key::A), save);
    assert(result.action == tb::UiAction::ResumeSuspended);

    // Starting another tower mode while suspended requires the safe-default warning.
    ui.update(fresh(tb::Key::Down), save); // Build City.
    result = ui.update(fresh(tb::Key::A), save);
    assert(result.action == tb::UiAction::None);
    assert(ui.scene() == tb::UiScene::OverwriteGameConfirm);
    assert(ui.selection() == 1); // No.
    result = ui.update(fresh(tb::Key::A), save);
    assert(result.action == tb::UiAction::None);
    assert(ui.scene() == tb::UiScene::MainMenu);
    assert(ui.selection() == 1);
    ui.update(fresh(tb::Key::A), save);
    assert(ui.scene() == tb::UiScene::OverwriteGameConfirm);
    ui.update(fresh(tb::Key::Up), save); // Yes.
    result = ui.update(fresh(tb::Key::A), save);
    assert(result.action == tb::UiAction::StartBuildCity);
    assert(ui.scene() == tb::UiScene::MainMenu);

    // High Scores table selector and safe clear confirmation.
    tb::insert_hall_score(save.hall_of_fame, tb::HallTable::QuickGame, 900, "OLD");
    tb::UiController hall_ui(save);
    hall_ui.update(fresh(tb::Key::A), save);
    hall_ui.update(fresh(tb::Key::Down), save);
    hall_ui.update(fresh(tb::Key::Down), save); // High Scores.
    hall_ui.update(fresh(tb::Key::A), save);
    assert(hall_ui.scene() == tb::UiScene::HighScoreSelect);
    assert(hall_ui.selection() == 0);
    hall_ui.update(fresh(tb::Key::A), save);
    assert(hall_ui.scene() == tb::UiScene::HighScoreTable);
    assert(hall_ui.selected_hall_table() == tb::HallTable::BuildCity);
    hall_ui.update(fresh(tb::Key::B), save);
    assert(hall_ui.scene() == tb::UiScene::HighScoreSelect);
    hall_ui.update(fresh(tb::Key::Down), save);
    hall_ui.update(fresh(tb::Key::Down), save); // Clear High Scores.
    hall_ui.update(fresh(tb::Key::A), save);
    assert(hall_ui.scene() == tb::UiScene::ClearHighScoresConfirm);
    assert(hall_ui.selection() == 1); // No.
    hall_ui.update(fresh(tb::Key::B), save);
    assert(hall_ui.scene() == tb::UiScene::HighScoreSelect);
    assert(save.hall_of_fame.tables[1][0].score == 900);
    hall_ui.update(fresh(tb::Key::Down), save);
    hall_ui.update(fresh(tb::Key::Down), save);
    hall_ui.update(fresh(tb::Key::A), save);
    assert(hall_ui.scene() == tb::UiScene::ClearHighScoresConfirm);
    hall_ui.update(fresh(tb::Key::Up), save);
    result = hall_ui.update(fresh(tb::Key::A), save);
    assert(result.save_dirty);
    assert(hall_ui.scene() == tb::UiScene::HighScoreSelect);
    assert(save.hall_of_fame.tables[1][0].score == 0);
    assert(std::strcmp(save.hall_of_fame.last_player_name.data(), "SUMEA") == 0);

    // Settings remains three rows and Reset City retains its safe confirmation behavior.
    tb::UiController settings_ui(save);
    settings_ui.update(fresh(tb::Key::A), save);
    settings_ui.update(fresh(tb::Key::Down), save);
    settings_ui.update(fresh(tb::Key::Down), save);
    settings_ui.update(fresh(tb::Key::Down), save); // Settings.
    settings_ui.update(fresh(tb::Key::A), save);
    assert(settings_ui.scene() == tb::UiScene::Settings);
    result = settings_ui.update(fresh(tb::Key::A), save);
    assert(result.save_dirty);
    assert(! settings_ui.sound_enabled());
    settings_ui.update(fresh(tb::Key::Down), save);
    settings_ui.update(fresh(tb::Key::A), save);
    assert(settings_ui.language() == 1);
    save.quick_best_population = 2222;
    save.city_tiles[12].type = 3;
    save.city_tiles[12].population = 800;
    save.city_tutorial_flags[7] = 1;
    settings_ui.update(fresh(tb::Key::Down), save);
    settings_ui.update(fresh(tb::Key::A), save);
    assert(settings_ui.scene() == tb::UiScene::ResetCityConfirm);
    assert(settings_ui.selection() == 1);
    settings_ui.update(fresh(tb::Key::Up), save);
    result = settings_ui.update(fresh(tb::Key::A), save);
    assert(result.save_dirty);
    assert(save.city_tiles[12].type == 0);
    assert(save.city_tutorial_flags[7] == 0);
    assert(save.quick_best_population == 2222);

    // Score submission: qualification -> name entry -> persisted table.
    tb::UiController score_ui(save);
    score_ui.begin_score_submission(tb::HallTable::QuickGame, 1234, save, tb::ScoreFlowReturn::RootMenu);
    assert(score_ui.scene() == tb::UiScene::ScoreQualification);
    assert(score_ui.pending_qualification().qualifies);
    assert(score_ui.pending_qualification().position == 1);
    assert(score_ui.pending_score() == 1234);
    score_ui.update(fresh(tb::Key::A), save);
    assert(score_ui.scene() == tb::UiScene::NameEntry);
    assert(std::strcmp(score_ui.name_entry().data(), "SUMEA") == 0);
    assert(score_ui.name_cursor() == 0);
    score_ui.update(fresh(tb::Key::A), save); // First typed A replaces SUMEA.
    assert(std::strcmp(score_ui.name_entry().data(), "A") == 0);
    result = score_ui.update(fresh(tb::Key::Start), save);
    assert(result.save_dirty);
    assert(score_ui.scene() == tb::UiScene::HighScoreTable);
    assert(save.hall_of_fame.tables[1][0].score == 1234);
    assert(std::strcmp(save.hall_of_fame.tables[1][0].name.data(), "A") == 0);
    assert(std::strcmp(save.hall_of_fame.last_player_name.data(), "A") == 0);
    result = score_ui.update(fresh(tb::Key::B), save);
    assert(result.action == tb::UiAction::None);
    assert(score_ui.scene() == tb::UiScene::MainMenu);

    // A non-qualifier never mutates the Hall table.
    const uint32_t previous_top = save.hall_of_fame.tables[1][0].score;
    score_ui.begin_score_submission(tb::HallTable::QuickGame, 0, save, tb::ScoreFlowReturn::RootMenu);
    assert(score_ui.scene() == tb::UiScene::ScoreFailure);
    result = score_ui.update(fresh(tb::Key::A), save);
    assert(! result.save_dirty);
    assert(score_ui.scene() == tb::UiScene::MainMenu);
    assert(save.hall_of_fame.tables[1][0].score == previous_top);

    // Build City score flow returns through an explicit runtime action.
    score_ui.begin_score_submission(tb::HallTable::BuildCity, 5000, save, tb::ScoreFlowReturn::BuildCity);
    score_ui.update(fresh(tb::Key::A), save);
    score_ui.update(fresh(tb::Key::A), save);
    result = score_ui.update(fresh(tb::Key::Start), save);
    assert(result.save_dirty);
    assert(score_ui.scene() == tb::UiScene::HighScoreTable);
    result = score_ui.update(fresh(tb::Key::B), save);
    assert(result.action == tb::UiAction::ReturnToBuildCity);

    // Instructions remains directly reachable from the root, and root B returns to Title.
    tb::UiController instructions_ui(save);
    instructions_ui.update(fresh(tb::Key::A), save);
    for(int index = 0; index < 4; ++index) instructions_ui.update(fresh(tb::Key::Down), save);
    instructions_ui.update(fresh(tb::Key::A), save);
    assert(instructions_ui.scene() == tb::UiScene::InstructionsMenu);
    instructions_ui.update(fresh(tb::Key::A), save);
    assert(instructions_ui.scene() == tb::UiScene::InstructionsPage);
    instructions_ui.update(fresh(tb::Key::B), save);
    instructions_ui.update(fresh(tb::Key::B), save);
    assert(instructions_ui.scene() == tb::UiScene::MainMenu);
    instructions_ui.update(fresh(tb::Key::B), save);
    assert(instructions_ui.scene() == tb::UiScene::Title);

    // Quick Game: recovered Java initialization and first 25 ms swing sample.
    tb::QuickGame quick;
    auto quick_snapshot = quick.snapshot();
    assert(quick_snapshot.status == tb::QuickGameStatus::Playing);
    assert(quick_snapshot.block_state == tb::QuickBlockState::Raising);
    assert(quick_snapshot.floor_count == 0);
        assert(quick_snapshot.chances_left == 3);
    assert(quick_snapshot.camera_y == 512);
    assert(quick_snapshot.camera_target_y == 512);
    assert(quick_snapshot.rope_length == 0);
    assert(quick_snapshot.swing_period_ms == 1550);
    assert(quick_snapshot.swing_amplitude_x == 128);
    assert(quick_snapshot.swing_amplitude_y == 64);
    assert(quick_snapshot.presentation_camera_y == 512);
    assert(quick_snapshot.tower_phase_tenths == 0);
    assert(quick_snapshot.tower_sway_amplitude == 0);
    assert(quick_snapshot.current_z_angle_degrees == 0);
    assert(! quick_snapshot.camera_impact_active);

    quick.update(25, {});
    quick_snapshot = quick.snapshot();
    assert(quick_snapshot.rope_length == 16);
    assert(quick_snapshot.current_x == 20);
    assert(quick_snapshot.current_y == 2480);
    assert(quick_snapshot.crane_x == quick_snapshot.current_x);
    assert(quick_snapshot.crane_y == quick_snapshot.current_y);
    assert(quick_snapshot.crane_angle_degrees == ((quick_snapshot.crane_x >> 4) * 2) / 3);
    assert(quick_snapshot.tower_phase_tenths == 25);
    assert(quick_snapshot.presentation_camera_y == quick_snapshot.camera_y);

    for(int index = 1; index < 104; ++index)
    {
        quick.update(25, {});
    }
    quick_snapshot = quick.snapshot();
    assert(quick_snapshot.rope_length == 1664);
    assert(quick_snapshot.block_state == tb::QuickBlockState::Attached);

    // A is fresh-drop only. A held without pressed does not release the block.
    tb::QuickGame held_only;
    for(int index = 0; index < 104; ++index)
    {
        held_only.update(25, {});
    }
    held_only.update(25, tb::InputFrame{tb::key_mask(tb::Key::A), 0});
    assert(held_only.snapshot().block_state == tb::QuickBlockState::Attached);

    const auto before_drop = quick.snapshot();
    quick.update(25, fresh(tb::Key::A));
    quick_snapshot = quick.snapshot();
    assert(quick_snapshot.block_state == tb::QuickBlockState::Falling);
    assert(quick_snapshot.current_y < before_drop.current_y + 128);
    assert(quick_snapshot.drop_velocity_x != 0 || quick_snapshot.drop_velocity_y != 0);
    assert(quick_snapshot.crane_x != quick_snapshot.current_x || quick_snapshot.crane_y != quick_snapshot.current_y);
    const int falling_crane_y = quick_snapshot.crane_y;
    quick.update(25, {});
    quick_snapshot = quick.snapshot();
    assert(quick_snapshot.block_state == tb::QuickBlockState::Falling);
    assert(quick_snapshot.crane_y != falling_crane_y || quick_snapshot.crane_x != before_drop.current_x);

    // The same input/delta stream must be bit-for-bit deterministic at the exposed state level.
    tb::QuickGame replay_a;
    tb::QuickGame replay_b;
    for(int index = 0; index < 104; ++index)
    {
        replay_a.update(25, {});
        replay_b.update(25, {});
    }
    replay_a.update(25, fresh(tb::Key::A));
    replay_b.update(25, fresh(tb::Key::A));
    for(int index = 0; index < 6; ++index)
    {
        replay_a.update(25, {});
        replay_b.update(25, {});
    }
    const auto replay_snapshot_a = replay_a.snapshot();
    const auto replay_snapshot_b = replay_b.snapshot();
    assert(replay_snapshot_a.current_x == replay_snapshot_b.current_x);
    assert(replay_snapshot_a.current_y == replay_snapshot_b.current_y);
    assert(replay_snapshot_a.drop_velocity_x == replay_snapshot_b.drop_velocity_x);
    assert(replay_snapshot_a.drop_velocity_y == replay_snapshot_b.drop_velocity_y);
    assert(replay_snapshot_a.swing_phase_ms == replay_snapshot_b.swing_phase_ms);


    // Landing threshold and Phase-6-facing accuracy categories are bytecode-pinned.
    struct LandingCase
    {
        int offset;
        tb::QuickAccuracyBand band;
    };
    constexpr LandingCase landing_cases[] = {
        {0, tb::QuickAccuracyBand::Perfect},
        {24, tb::QuickAccuracyBand::Perfect},
        {25, tb::QuickAccuracyBand::Great},
        {49, tb::QuickAccuracyBand::Great},
        {50, tb::QuickAccuracyBand::Good},
        {79, tb::QuickAccuracyBand::Good},
        {80, tb::QuickAccuracyBand::Ok},
        {127, tb::QuickAccuracyBand::Ok},
    };
    for(const auto& landing_case : landing_cases)
    {
        tb::QuickGame landing;
        landing.debug_resolve_landing_for_test(landing_case.offset);
        const auto landing_snapshot = landing.snapshot();
        assert(landing_snapshot.floor_count == 1);
        assert(landing_snapshot.last_accuracy == landing_case.band);
        assert(landing_snapshot.block_state == tb::QuickBlockState::Settled);
        assert(landing.floor(0).x == landing_case.offset);
        assert(landing.floor(0).y == 128);
        assert(landing.floor(0).offset == landing_case.offset);
    }

    tb::QuickGame edge_failure;
    edge_failure.debug_resolve_landing_for_test(128);
    assert(edge_failure.floor_count() == 0);
    assert(edge_failure.snapshot().block_state == tb::QuickBlockState::Slipping);
    assert(edge_failure.snapshot().last_accuracy == tb::QuickAccuracyBand::None);
    edge_failure.update(25, {});
    assert(edge_failure.snapshot().current_z_angle_degrees == -2);
    // House gives every slipping block an independent random +/-60 degree
    // Y-axis target and interpolates it over the same 500 ms window.
    assert(edge_failure.snapshot().current_y_angle_degrees == 3 ||
           edge_failure.snapshot().current_y_angle_degrees == -3);

    // Presentation pose is deterministic and layered on top of collision coordinates only.
    tb::QuickGame rocking;
    rocking.debug_resolve_landing_for_test(0);
    rocking.debug_resolve_landing_for_test(10);
    rocking.debug_resolve_landing_for_test(20);
    rocking.debug_resolve_landing_for_test(-30);
    rocking.debug_resolve_landing_for_test(60);
    auto rocking_snapshot = rocking.snapshot();
    assert(rocking_snapshot.floor_count == 5);
    assert(rocking_snapshot.tower_sway_amplitude == 4);
    assert(rocking_snapshot.tower_global_x == 13);
    const tb::QuickFloorRenderPose initial_top_pose = rocking.floor_render_pose(4);
    assert(initial_top_pose.x_delta == 33);
    assert(initial_top_pose.y_delta == -5);
    assert(initial_top_pose.z_angle_degrees == -10);
    assert(rocking.floor(4).x == 60);
    assert(rocking.floor(4).y == 1152);

    rocking.update(25, {});
    rocking_snapshot = rocking.snapshot();
    assert(rocking_snapshot.tower_phase_tenths == 25);
    const tb::QuickFloorRenderPose moving_top_pose = rocking.floor_render_pose(4);
    assert(moving_top_pose.x_delta == 37);
    assert(moving_top_pose.y_delta == -6);
    assert(moving_top_pose.z_angle_degrees == -12);
    assert(rocking.floor(4).x == 60);  // presentation must not mutate collision state

    // Miss impact shakes only presentation camera and expires after the original 800 ms window.
    tb::QuickGame impact;
    impact.debug_set_falling_state_for_test(500, -500, 0, 0);
    impact.update(25, {});
    assert(impact.snapshot().camera_impact_active);
    const int impact_delta = impact.snapshot().presentation_camera_y - impact.snapshot().camera_y;
    assert(impact_delta >= -31 && impact_delta <= 32);
    for(int index = 0; index < 32; ++index)
    {
        impact.update(25, {});
    }
    assert(! impact.snapshot().camera_impact_active);
    assert(impact.snapshot().presentation_camera_y == impact.snapshot().camera_y);

    // Quick Game uses the recovered non-Build-City endless difficulty branch (L=4).
    tb::QuickGame progression;
    progression.debug_resolve_landing_for_test(0);
    auto progression_snapshot = progression.snapshot();
    assert(progression_snapshot.swing_period_ms == 1668);
    assert(progression_snapshot.swing_amplitude_x == 215);
    assert(progression_snapshot.swing_amplitude_y == 86);
    assert(progression_snapshot.camera_target_y == 512);
    progression.debug_resolve_landing_for_test(0);
    progression_snapshot = progression.snapshot();
    assert(progression_snapshot.floor_count == 2);
    assert(progression_snapshot.swing_period_ms == 1666);
    assert(progression_snapshot.swing_amplitude_x == 218);
    assert(progression_snapshot.swing_amplitude_y == 87);
    assert(progression_snapshot.camera_target_y == 768);

    tb::QuickGame forty_floors;
    for(int index = 0; index < 40; ++index)
    {
        forty_floors.debug_resolve_landing_for_test(0);
    }
    const auto forty_snapshot = forty_floors.snapshot();
    assert(forty_snapshot.floor_count == 40);
    assert(forty_snapshot.status == tb::QuickGameStatus::Playing);
    assert(forty_snapshot.swing_period_ms == 1582);
    assert(forty_snapshot.swing_amplitude_x == 327);
    assert(forty_snapshot.swing_amplitude_y == 141);

    // Population and combo accounting mirror House.k(int), House.r(), and the frame timer.
    tb::QuickGame perfect_scoring;
    perfect_scoring.debug_resolve_landing_for_test(0);
    auto scoring_snapshot = perfect_scoring.snapshot();
    assert(scoring_snapshot.population == 4);
    assert(scoring_snapshot.last_population_award == 4);
    assert(scoring_snapshot.combo_count == 1);
    assert(scoring_snapshot.combo_bonus_pending == 2);
    assert(scoring_snapshot.combo_meter_ms == 6000);
    assert(scoring_snapshot.longest_combo == 0);

    tb::QuickGame great_scoring;
    great_scoring.debug_resolve_landing_for_test(25);
    scoring_snapshot = great_scoring.snapshot();
    assert(scoring_snapshot.population == 3);
    assert(scoring_snapshot.combo_count == 1);
    assert(scoring_snapshot.combo_bonus_pending == 0);
    assert(scoring_snapshot.combo_meter_ms == -2000);

    tb::QuickGame height_bonus;
    for(int index = 0; index < 10; ++index)
    {
        height_bonus.debug_resolve_landing_for_test(80);
    }
    assert(height_bonus.snapshot().population == 10);
    height_bonus.debug_resolve_landing_for_test(80);
    assert(height_bonus.snapshot().population == 12);

    tb::QuickGame combo_scoring;
    combo_scoring.debug_resolve_landing_for_test(0);
    combo_scoring.debug_resolve_landing_for_test(25);
    scoring_snapshot = combo_scoring.snapshot();
    assert(scoring_snapshot.population == 7);
    assert(scoring_snapshot.combo_count == 2);
    assert(scoring_snapshot.combo_bonus_pending == 6);
    assert(scoring_snapshot.combo_meter_ms == 6000);
    assert(scoring_snapshot.longest_combo == 2);
    combo_scoring.update(25, {});
    assert(combo_scoring.snapshot().combo_meter_ms == 5971);

    for(int guard = 0; guard < 400 && combo_scoring.snapshot().combo_count != 0; ++guard)
    {
        combo_scoring.update(25, {});
    }
    scoring_snapshot = combo_scoring.snapshot();
    assert(scoring_snapshot.combo_count == 0);
    assert(scoring_snapshot.combo_meter_ms == 0);
    assert(scoring_snapshot.population == 13);
    assert(scoring_snapshot.combo_bonus_pending == 6);
    assert(scoring_snapshot.last_population_award == 6);

    tb::QuickGame combo_miss;
    combo_miss.debug_resolve_landing_for_test(0);
    combo_miss.debug_set_falling_state_for_test(500, -500, 0, 0);
    combo_miss.update(25, {});
    scoring_snapshot = combo_miss.snapshot();
    assert(scoring_snapshot.population == 6);
    assert(scoring_snapshot.combo_count == 0);
    assert(scoring_snapshot.combo_meter_ms == -25);
    assert(scoring_snapshot.chances_left == 2);

    // Falling below the screen consumes a construction chance, then respawns after 400 ms.
    tb::QuickGame miss_flow;
    miss_flow.debug_set_falling_state_for_test(500, -500, 0, 0);
    miss_flow.update(25, {});
    assert(miss_flow.snapshot().chances_left == 2);
    assert(miss_flow.snapshot().block_state == tb::QuickBlockState::Missed);
    for(int index = 0; index < 15; ++index)
    {
        miss_flow.update(25, {});
    }
    assert(miss_flow.snapshot().block_state == tb::QuickBlockState::Missed);
    miss_flow.update(25, {});
    assert(miss_flow.snapshot().block_state == tb::QuickBlockState::Attached);

    // Three misses end the run and never add floors.
    tb::QuickGame game_over;
    for(int miss = 0; miss < 3; ++miss)
    {
        game_over.debug_set_falling_state_for_test(500, -500, 0, 0);
        game_over.update(25, {});
        if(miss < 2)
        {
            for(int index = 0; index < 16; ++index)
            {
                game_over.update(25, {});
            }
        }
    }
    assert(game_over.floor_count() == 0);
    assert(game_over.snapshot().chances_left == 0);
    assert(game_over.snapshot().status == tb::QuickGameStatus::GameOver);
    assert(game_over.snapshot().camera_impact_active);
    assert(game_over.snapshot().tower_sway_wave == 0);
    for(int index = 0; index < 32; ++index)
    {
        game_over.update(25, {});
    }
    assert(! game_over.snapshot().camera_impact_active);
    assert(game_over.snapshot().presentation_camera_y == game_over.snapshot().camera_y);
    for(int index = 32; index < 79; ++index)
    {
        game_over.update(25, {});
    }
    assert(game_over.snapshot().status == tb::QuickGameStatus::GameOver);
    game_over.update(25, {});
    assert(game_over.snapshot().status == tb::QuickGameStatus::Results);
    const tb::QuickGameResult empty_result = game_over.result();
    assert(empty_result.population == 0);
    assert(empty_result.height == 0);
    assert(empty_result.longest_combo == 0);

    tb::QuickGame scored_result;
    scored_result.debug_resolve_landing_for_test(0);
    for(int miss = 0; miss < 3; ++miss)
    {
        scored_result.debug_set_falling_state_for_test(500, -500, 0, 0);
        scored_result.update(25, {});
        if(miss < 2)
        {
            for(int index = 0; index < 16; ++index)
            {
                scored_result.update(25, {});
            }
        }
    }
    for(int index = 0; index < 80; ++index)
    {
        scored_result.update(25, {});
    }
    const tb::QuickGameResult result_stats = scored_result.result();
    assert(scored_result.snapshot().status == tb::QuickGameStatus::Results);
    assert(result_stats.population == 6);
    assert(result_stats.height == 1);
    assert(result_stats.longest_combo == 0);

    // Build City: empty city starts at the canonical center and Residential is available.
    tb::SaveData city_save = tb::make_default_save();
    tb::BuildCity city(city_save);
    auto city_snapshot = city.snapshot();
    assert(city_snapshot.mode == tb::BuildCityMode::Browse);
    assert(city_snapshot.total_population == 0);
    assert(city_snapshot.milestone == 0);
    assert(city_snapshot.city_level == 0);
    assert(city_snapshot.max_unlocked_building_type == 1);
    assert(city_snapshot.max_trophy_building_type == 0);
    assert(city_snapshot.selected_building_type == 1);
    assert(city_snapshot.selected_unlock_population == 0);
    assert(city_snapshot.cursor_column == 2);
    assert(city_snapshot.cursor_row == 2);

    // Browse selection can inspect all four types, but locked types cannot launch construction.
    city.update(16, fresh(tb::Key::Down), city_save);
    assert(city.snapshot().selected_building_type == 2);
    assert(city.snapshot().selected_unlock_population == 250);
    city.update(16, fresh(tb::Key::Down), city_save);
    assert(city.snapshot().selected_building_type == 3);
    assert(city.snapshot().selected_unlock_population == 800);
    city.update(16, fresh(tb::Key::Up), city_save);
    auto city_update = city.update(16, fresh(tb::Key::A), city_save);
    assert(! city_update.save_dirty);
    assert(! city.construction_request().pending);
    city.update(16, fresh(tb::Key::Up), city_save);
    city.update(16, fresh(tb::Key::A), city_save);
    auto construction = city.construction_request();
    assert(construction.pending);
    assert(construction.building_type == 1);
    assert(construction.target_height == 10);
    assert(! construction.trophy_eligible);
    city.clear_construction_request();

    // Exact unlock and trophy thresholds are derived from saved population.
    tb::SaveData unlocked_save = tb::make_default_save();
    unlocked_save.city_tiles[0].type = 1;
    unlocked_save.city_tiles[0].population = 1400;
    tb::BuildCity unlocked_city(unlocked_save);
    assert(unlocked_city.snapshot().milestone == 8);
    assert(unlocked_city.snapshot().max_unlocked_building_type == 3);
    assert(unlocked_city.snapshot().max_trophy_building_type == 1);
    assert(unlocked_city.snapshot().selected_building_type == 3);
    unlocked_city.update(16, fresh(tb::Key::Up), unlocked_save);
    unlocked_city.update(16, fresh(tb::Key::Up), unlocked_save);
    unlocked_city.update(16, fresh(tb::Key::A), unlocked_save);
    construction = unlocked_city.construction_request();
    assert(construction.pending);
    assert(construction.building_type == 1);
    assert(construction.target_height == 10);
    assert(construction.trophy_eligible);

    // A constructed Residential tower enters placement at (2,2) and commits after 3000 ms.
    tb::SaveData placement_save = tb::make_default_save();
    tb::BuildCity placement(placement_save);
    placement.accept_constructed_tower(1, 100, 1);
    placement.update(750, {}, placement_save);
    assert(placement.snapshot().mode == tb::BuildCityMode::Placement);
    assert(placement.snapshot().cursor_column == 2);
    assert(placement.snapshot().cursor_row == 2);
    assert(placement.snapshot().placement_valid);
    city_update = placement.update(16, fresh(tb::Key::A), placement_save);
    assert(! city_update.save_dirty);
    assert(placement.snapshot().placement_committing);
    placement.update(2999, {}, placement_save);
    assert(placement.snapshot().mode == tb::BuildCityMode::Placement);
    city_update = placement.update(2, {}, placement_save);
    assert(city_update.save_dirty);
    assert(placement.snapshot().mode == tb::BuildCityMode::Browse);
    assert(placement_save.city_tiles[12].type == 1);
    assert(placement_save.city_tiles[12].population == 100);
    assert(placement_save.city_tiles[12].roof == 1);
    assert(placement.snapshot().total_population == 100);

    // Replacement is legal and population changes by new minus old.
    placement.accept_constructed_tower(1, 40, 2);
    placement.update(750, {}, placement_save);
    placement.update(16, fresh(tb::Key::A), placement_save);
    assert(placement.snapshot().last_population_delta == -60);
    city_update = placement.update(3001, {}, placement_save);
    assert(city_update.save_dirty);
    assert(placement_save.city_tiles[12].population == 40);
    assert(placement_save.city_tiles[12].roof == 2);
    assert(placement.snapshot().total_population == 40);

    // Commercial requires a cardinal Residential neighbor; a diagonal does not count.
    tb::SaveData rules_save = tb::make_default_save();
    rules_save.city_tiles[11].type = 1;  // left of center
    rules_save.city_tiles[11].population = 250;
    tb::BuildCity rules(rules_save);
    rules.accept_constructed_tower(2, 75, 1);
    rules.update(750, {}, rules_save);
    assert(rules.snapshot().placement_valid);
    rules.update(16, fresh(tb::Key::Right), rules_save); // (3,2): center is empty, left center isn't type 1
    assert(! rules.snapshot().placement_valid);

    // Left from column 0 enters the original demolition/discard selector at row 4.
    tb::BuildCity discard(rules_save);
    discard.accept_constructed_tower(1, 999, 1);
    discard.update(750, {}, rules_save);
    discard.update(16, fresh(tb::Key::Left), rules_save);
    discard.update(16, fresh(tb::Key::Left), rules_save);
    discard.update(16, fresh(tb::Key::Left), rules_save);
    assert(discard.snapshot().cursor_column == -1);
    assert(discard.snapshot().cursor_row == 4);
    assert(discard.snapshot().placement_valid);
    discard.update(16, fresh(tb::Key::A), rules_save);
    city_update = discard.update(3001, {}, rules_save);
    assert(! city_update.save_dirty);
    assert(discard.snapshot().mode == tb::BuildCityMode::Browse);
    assert(discard.snapshot().total_population == 250);

    // City-level boundaries include the original level-zero placeholder before Tiny Town.
    tb::SaveData mega_save = tb::make_default_save();
    mega_save.city_tiles[0].type = 1;
    mega_save.city_tiles[0].population = 19000;
    tb::BuildCity mega(mega_save);
    assert(mega.snapshot().milestone == 20);
    assert(mega.snapshot().city_level == 9);
    assert(mega.snapshot().max_unlocked_building_type == 4);
    assert(mega.snapshot().max_trophy_building_type == 4);

    std::cout << "runtime core ok\n";
    return 0;
}
