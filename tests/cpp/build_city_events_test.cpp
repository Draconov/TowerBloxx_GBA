#include <cassert>
#include <iostream>

#include "tb/build_city_events.h"
#include "tb/save_data.h"

namespace
{
void acknowledge_all(tb::BuildCityEventController& events, tb::SaveData& save)
{
    int guard = 0;
    while(events.has_event())
    {
        const int id = events.current_event().id;
        assert(id >= 0 && id < 46);
        assert(save.city_tutorial_flags[id] == 0);
        assert(events.acknowledge(save));
        assert(save.city_tutorial_flags[id] == 1);
        assert(++guard < 60);
    }
}
}

int main()
{
    constexpr int expected_localization[46] = {
        36, 37, 38, 39, 40, 41,
        43, 43, 43, 58, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 59, 43, 60,
        45, 44, 44, 44, 44, 44, 44, 44, 44,
        42, 47, 49, 50, 48, 46,
        51, 51, 51, 51,
        52, 53, 54,
    };
    for(int id = 0; id < 46; ++id)
    {
        const tb::BuildCityEventDefinition definition = tb::build_city_event_definition(id);
        assert(definition.id == id);
        assert(definition.localization_index == expected_localization[id]);
    }

    tb::SaveData save = tb::make_default_save();
    tb::BuildCityEventController events;
    tb::BuildCityProgressState fresh{};
    events.on_city_entered(fresh, save);
    assert(events.has_event());
    assert(events.current_event().id == 0);
    assert(events.pending_count() == 3);
    acknowledge_all(events, save);
    assert(save.city_tutorial_flags[0] == 1);
    assert(save.city_tutorial_flags[1] == 1);
    assert(save.city_tutorial_flags[2] == 1);

    // Entry-only onboarding does not flood an existing Fix-10 city whose flags were unused.
    tb::SaveData progressed = tb::make_default_save();
    tb::BuildCityProgressState existing{};
    existing.total_population = 800;
    existing.milestone = 6;
    existing.city_level = 2;
    existing.max_unlocked_building_type = 3;
    existing.occupied_tiles = 4;
    tb::BuildCityEventController migrated_entry;
    migrated_entry.on_city_entered(existing, progressed);
    assert(! migrated_entry.has_event());

    // First construction return shows placement guidance. Milestone 5 additionally shows comparison icons.
    tb::SaveData construction_save = tb::make_default_save();
    tb::BuildCityEventController construction_events;
    tb::BuildCityProgressState pre_first{};
    construction_events.on_constructed_tower_accepted(pre_first, construction_save);
    assert(construction_events.pending_count() == 1);
    assert(construction_events.current_event().id == 3);
    acknowledge_all(construction_events, construction_save);

    tb::BuildCityProgressState milestone5{};
    milestone5.total_population = 600;
    milestone5.milestone = 5;
    milestone5.city_level = 2;
    milestone5.max_unlocked_building_type = 2;
    milestone5.occupied_tiles = 5;
    construction_events.on_constructed_tower_accepted(milestone5, construction_save);
    assert(construction_events.pending_count() == 1);
    assert(construction_events.current_event().id == 38);
    acknowledge_all(construction_events, construction_save);

    // A huge population jump must not lose intermediate original events.
    tb::SaveData jump_save = tb::make_default_save();
    for(int index = 0; index <= 4; ++index) jump_save.city_tutorial_flags[index] = 1;
    tb::BuildCityProgressState before{};
    before.total_population = 0;
    before.milestone = 0;
    before.city_level = 0;
    before.max_unlocked_building_type = 1;
    before.max_trophy_building_type = 0;
    before.occupied_tiles = 0;
    tb::BuildCityProgressState after{};
    after.total_population = 2200;
    after.milestone = 10;
    after.city_level = 4;
    after.max_unlocked_building_type = 4;
    after.max_trophy_building_type = 1;
    after.occupied_tiles = 1;
    tb::BuildCityEventController jump_events;
    jump_events.on_placement_committed(before, after, jump_save);
    const int expected_jump_ids[] = {
        5, 24,        // milestone 1
        6, 33,        // milestone 2
        7, 34, 37,    // milestone 3 + Commercial + neighbor rule
        8, 25,        // milestone 4 + Small Town
        9,            // milestone 5 national news
        10, 35,       // milestone 6 + Office
        11, 26,       // milestone 7 + Town
        12, 39,       // milestone 8 + Residential trophy roofs
        13, 27,       // milestone 9 + Small City
        14, 36,       // milestone 10 + Luxury
    };
    for(int expected_id : expected_jump_ids)
    {
        assert(jump_events.has_event());
        const tb::BuildCityEvent event = jump_events.current_event();
        assert(event.id == expected_id);
        if(expected_id >= 5 && expected_id <= 23)
        {
            assert(event.number_value > 0);
        }
        jump_events.acknowledge(jump_save);
    }
    assert(! jump_events.has_event());

    // Trophy unlocks and special milestone messages preserve their recovered IDs/args.
    tb::SaveData trophy_save = tb::make_default_save();
    for(auto& flag : trophy_save.city_tutorial_flags) flag = 1;
    trophy_save.city_tutorial_flags[40] = 0;
    tb::BuildCityProgressState trophy_before{};
    trophy_before.milestone = 11;
    trophy_before.city_level = 5;
    trophy_before.max_unlocked_building_type = 4;
    trophy_before.max_trophy_building_type = 1;
    tb::BuildCityProgressState trophy_after = trophy_before;
    trophy_after.milestone = 12;
    trophy_after.total_population = 4000;
    trophy_after.max_trophy_building_type = 2;
    tb::BuildCityEventController trophy_events;
    trophy_events.on_placement_committed(trophy_before, trophy_after, trophy_save);
    assert(trophy_events.pending_count() == 1);
    assert(trophy_events.current_event().id == 40);
    assert(trophy_events.current_event().text_index0 == 88);

    // Final milestone, final city promotion, full grid/parade, then ultimate goal.
    tb::SaveData final_save = tb::make_default_save();
    for(auto& flag : final_save.city_tutorial_flags) flag = 1;
    for(int id : {32, 43, 44, 45}) final_save.city_tutorial_flags[id] = 0;
    tb::BuildCityProgressState final_before{};
    final_before.total_population = 17000;
    final_before.milestone = 19;
    final_before.city_level = 8;
    final_before.max_unlocked_building_type = 4;
    final_before.max_trophy_building_type = 4;
    final_before.occupied_tiles = 24;
    tb::BuildCityProgressState final_after = final_before;
    final_after.total_population = 19000;
    final_after.milestone = 20;
    final_after.city_level = 9;
    final_after.occupied_tiles = 25;
    tb::BuildCityEventController final_events;
    final_events.on_placement_committed(final_before, final_after, final_save);
    const int expected_final[] = {43, 32, 44, 45};
    for(int id : expected_final)
    {
        assert(final_events.current_event().id == id);
        const tb::BuildCityEvent event = final_events.current_event();
        if(id == 44) assert(event.text_index0 == 79);
        if(id == 45) assert(event.number_value == 19000);
        final_events.acknowledge(final_save);
    }
    assert(! final_events.has_event());


    // A power loss between queued messages must reconstruct the unseen remainder on next city entry.
    tb::SaveData recovery_save = tb::make_default_save();
    for(auto& flag : recovery_save.city_tutorial_flags) flag = 1;
    recovery_save.city_tutorial_flags[37] = 0; // Commercial neighbor-rule follow-up was not acknowledged yet.
    tb::BuildCityProgressState recovery_state{};
    recovery_state.total_population = 250;
    recovery_state.milestone = 3;
    recovery_state.city_level = 1;
    recovery_state.max_unlocked_building_type = 2;
    recovery_state.occupied_tiles = 2;
    tb::BuildCityEventController recovery_events;
    recovery_events.on_city_entered(recovery_state, recovery_save);
    assert(recovery_events.pending_count() == 1);
    assert(recovery_events.current_event().id == 37);

    // Same recovery rule applies to the post-parade ultimate-goal message.
    tb::SaveData parade_recovery = tb::make_default_save();
    for(auto& flag : parade_recovery.city_tutorial_flags) flag = 1;
    parade_recovery.city_tutorial_flags[45] = 0;
    tb::BuildCityProgressState full_city{};
    full_city.total_population = 19000;
    full_city.milestone = 20;
    full_city.city_level = 9;
    full_city.max_unlocked_building_type = 4;
    full_city.max_trophy_building_type = 4;
    full_city.occupied_tiles = 25;
    tb::BuildCityEventController parade_recovery_events;
    parade_recovery_events.on_city_entered(full_city, parade_recovery);
    assert(parade_recovery_events.pending_count() == 1);
    assert(parade_recovery_events.current_event().id == 45);

    // Already-acknowledged events never enqueue twice.
    tb::BuildCityEventController repeat;
    repeat.on_placement_committed(final_before, final_after, final_save);
    assert(! repeat.has_event());

    std::cout << "build city events ok\n";
}
