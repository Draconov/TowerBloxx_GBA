#include <cassert>
#include <cstdint>
#include <iostream>

#include "tb/app_state.h"
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

    save = tb::make_default_save();
    tb::UiController ui(save);
    assert(ui.scene() == tb::UiScene::Title);
    assert(ui.selection() == 0);
    assert(ui.language() == 0);
    assert(ui.sound_enabled());
    assert(ui.pending_game_request() == tb::GameRequest::None);

    auto result = ui.update(fresh(tb::Key::A), save);
    assert(! result.save_dirty);
    assert(ui.scene() == tb::UiScene::MainMenu);
    assert(ui.selection() == 0);

    // Main menu: New game, Settings, Instructions, About; navigation wraps.
    ui.update(fresh(tb::Key::Up), save);
    assert(ui.selection() == 3);
    ui.update(fresh(tb::Key::Down), save);
    assert(ui.selection() == 0);

    // New Game -> Quick Game / Build City.
    ui.update(fresh(tb::Key::A), save);
    assert(ui.scene() == tb::UiScene::NewGameMenu);
    assert(ui.selection() == 0);
    ui.update(fresh(tb::Key::A), save);
    assert(ui.pending_game_request() == tb::GameRequest::QuickGame);
    ui.clear_game_request();
    assert(ui.pending_game_request() == tb::GameRequest::None);
    ui.update(fresh(tb::Key::Down), save);
    assert(ui.selection() == 1);
    ui.update(fresh(tb::Key::A), save);
    assert(ui.pending_game_request() == tb::GameRequest::BuildCity);
    ui.clear_game_request();
    ui.update(fresh(tb::Key::B), save);
    assert(ui.scene() == tb::UiScene::MainMenu);
    assert(ui.selection() == 0);

    // Settings toggles sound and cycles exactly five languages, returning dirty state.
    ui.update(fresh(tb::Key::Down), save);
    assert(ui.selection() == 1);
    ui.update(fresh(tb::Key::A), save);
    assert(ui.scene() == tb::UiScene::Settings);
    assert(ui.selection() == 0);
    result = ui.update(fresh(tb::Key::A), save);
    assert(result.save_dirty);
    assert(! ui.sound_enabled());
    assert(save.sound_enabled == 0);
    assert(! tb::valid_save(save));
    tb::finalize_save(save);
    assert(tb::valid_save(save));

    ui.update(fresh(tb::Key::Down), save);
    assert(ui.selection() == 1);
    result = ui.update(fresh(tb::Key::A), save);
    assert(result.save_dirty);
    assert(ui.language() == 1);
    assert(save.language == 1);
    ui.update(fresh(tb::Key::Left), save);
    assert(ui.language() == 0);
    ui.update(fresh(tb::Key::Left), save);
    assert(ui.language() == 4);
    ui.update(fresh(tb::Key::Right), save);
    assert(ui.language() == 0);
    ui.update(fresh(tb::Key::B), save);
    assert(ui.scene() == tb::UiScene::MainMenu);

    // Instructions -> Quick/City page and B unwinds one level at a time.
    ui.update(fresh(tb::Key::Down), save); // Settings
    ui.update(fresh(tb::Key::Down), save); // Instructions
    assert(ui.selection() == 2);
    ui.update(fresh(tb::Key::A), save);
    assert(ui.scene() == tb::UiScene::InstructionsMenu);
    assert(ui.selection() == 0);
    ui.update(fresh(tb::Key::A), save);
    assert(ui.scene() == tb::UiScene::InstructionsPage);
    assert(ui.instructions_page() == 0);
    ui.update(fresh(tb::Key::B), save);
    assert(ui.scene() == tb::UiScene::InstructionsMenu);
    ui.update(fresh(tb::Key::Down), save);
    ui.update(fresh(tb::Key::A), save);
    assert(ui.scene() == tb::UiScene::InstructionsPage);
    assert(ui.instructions_page() == 1);
    ui.update(fresh(tb::Key::B), save);
    ui.update(fresh(tb::Key::B), save);
    assert(ui.scene() == tb::UiScene::MainMenu);

    // About is the fourth main menu item, and Main B returns to Title.
    ui.update(fresh(tb::Key::Up), save);
    assert(ui.selection() == 3);
    ui.update(fresh(tb::Key::A), save);
    assert(ui.scene() == tb::UiScene::About);
    ui.update(fresh(tb::Key::B), save);
    assert(ui.scene() == tb::UiScene::MainMenu);
    ui.update(fresh(tb::Key::B), save);
    assert(ui.scene() == tb::UiScene::Title);


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

    quick.update(25, {});
    quick_snapshot = quick.snapshot();
    assert(quick_snapshot.rope_length == 16);
    assert(quick_snapshot.current_x == 20);
    assert(quick_snapshot.current_y == 2480);

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
    for(int index = 0; index < 79; ++index)
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

    std::cout << "runtime core ok\n";
    return 0;
}
