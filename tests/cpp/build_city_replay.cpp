#include <cassert>
#include <cstdint>
#include <iostream>

#include "tb/app_state.h"
#include "tb/build_city.h"
#include "tb/save_data.h"

namespace
{
tb::InputFrame fresh(tb::Key key)
{
    return tb::InputFrame{tb::key_mask(key), tb::key_mask(key)};
}

void press(tb::BuildCity& city, tb::SaveData& save, tb::Key key)
{
    const tb::BuildCityUpdateResult result = city.update(16, fresh(key), save);
    assert(! result.exit);
}

void move_to(tb::BuildCity& city, tb::SaveData& save, int column, int row)
{
    auto snapshot = city.snapshot();
    while(snapshot.cursor_row > row)
    {
        press(city, save, tb::Key::Up);
        snapshot = city.snapshot();
    }
    while(snapshot.cursor_row < row)
    {
        press(city, save, tb::Key::Down);
        snapshot = city.snapshot();
    }
    while(snapshot.cursor_column > column)
    {
        press(city, save, tb::Key::Left);
        snapshot = city.snapshot();
    }
    while(snapshot.cursor_column < column)
    {
        press(city, save, tb::Key::Right);
        snapshot = city.snapshot();
    }
}

void place(
        tb::BuildCity& city, tb::SaveData& save, uint8_t type, int population, uint8_t roof,
        int column, int row)
{
    city.accept_constructed_tower(type, population, roof);
    city.update(750, {}, save);
    move_to(city, save, column, row);
    assert(city.snapshot().placement_valid);
    press(city, save, tb::Key::A);
    assert(city.snapshot().placement_committing);
    const tb::BuildCityUpdateResult result = city.update(3001, {}, save);
    assert(result.save_dirty);
    assert(result.placement_committed);
    assert(result.committed_total_population == city.snapshot().total_population);
    const tb::BuildCityUpdateResult after_commit = city.update(16, {}, save);
    assert(! after_commit.placement_committed);
    assert(after_commit.committed_total_population == 0);
    assert(city.snapshot().mode == tb::BuildCityMode::Browse);
}

int request_target(tb::BuildCity& city, tb::SaveData& save)
{
    press(city, save, tb::Key::A);
    const tb::BuildCityConstructionRequest request = city.construction_request();
    assert(request.pending);
    const int target = request.target_height;
    city.clear_construction_request();
    return target;
}

void print_state(const char* label, const tb::BuildCity& city)
{
    const tb::BuildCitySnapshot snapshot = city.snapshot();
    std::cout << label << ' ' << snapshot.total_population << ' ' << snapshot.milestone << ' '
              << snapshot.city_level << ' ' << snapshot.max_unlocked_building_type << ' '
              << snapshot.selected_building_type << '\n';
}
}

int main()
{
    tb::SaveData save = tb::make_default_save();
    tb::BuildCity city(save);

    auto snapshot = city.snapshot();
    assert(snapshot.total_population == 0);
    assert(snapshot.milestone == 0);
    assert(snapshot.city_level == 0);
    assert(snapshot.max_unlocked_building_type == 1);
    const int initial_target = request_target(city, save);
    assert(initial_target == 10);
    std::cout << "START 0 0 0 1 " << initial_target << '\n';

    // Residential can be placed anywhere.  The first placement crosses 75 and Small Town.
    place(city, save, 1, 100, 1, 2, 2);
    print_state("PLACE_BLUE_100", city);
    snapshot = city.snapshot();
    assert(snapshot.total_population == 100);
    assert(snapshot.milestone == 1);
    assert(snapshot.city_level == 1);

    // Replacement is population-delta based.  260 unlocks Commercial at milestone 3.
    place(city, save, 1, 260, 2, 2, 2);
    snapshot = city.snapshot();
    assert(snapshot.total_population == 260);
    assert(snapshot.milestone == 3);
    assert(snapshot.max_unlocked_building_type == 2);
    assert(snapshot.selected_building_type == 2);
    const int commercial_target = request_target(city, save);
    assert(commercial_target == 20);
    std::cout << "UNLOCK 2 " << commercial_target << '\n';

    // Commercial is legal next to the center Residential tile.
    place(city, save, 2, 200, 1, 3, 2);
    snapshot = city.snapshot();
    assert(snapshot.total_population == 460);
    assert(snapshot.milestone == 4);
    assert(snapshot.city_level == 2);
    print_state("PLACE_RED_200", city);

    // A second Residential reaches 800 and unlocks Office.
    place(city, save, 1, 340, 0, 1, 2);
    snapshot = city.snapshot();
    assert(snapshot.total_population == 800);
    assert(snapshot.milestone == 6);
    assert(snapshot.max_unlocked_building_type == 3);
    assert(snapshot.selected_building_type == 3);
    const int office_target = request_target(city, save);
    assert(office_target == 30);
    std::cout << "UNLOCK 3 " << office_target << '\n';

    // Replacing the center with Office is legal because blue and red are cardinal neighbors.
    place(city, save, 3, 500, 2, 2, 2);
    snapshot = city.snapshot();
    assert(snapshot.total_population == 1040);
    assert(snapshot.milestone == 7);
    assert(snapshot.city_level == 3);
    print_state("PLACE_GREEN_500", city);

    // Raise population by replacing Residential while keeping blue/red neighbors intact.
    place(city, save, 1, 1500, 2, 1, 2);
    snapshot = city.snapshot();
    assert(snapshot.total_population == 2200);
    assert(snapshot.milestone == 10);
    assert(snapshot.city_level == 4);
    assert(snapshot.max_unlocked_building_type == 4);
    assert(snapshot.selected_building_type == 4);
    const int luxury_target = request_target(city, save);
    assert(luxury_target == 40);
    std::cout << "UNLOCK 4 " << luxury_target << '\n';
    std::cout << "FINAL " << snapshot.total_population << ' ' << snapshot.milestone << ' '
              << snapshot.city_level << ' ' << snapshot.max_unlocked_building_type << ' '
              << snapshot.selected_building_type << '\n';

    // The original column=-1 selector discards the newly built tower; it does not erase a saved city tile.
    const auto city_before_discard = save.city_tiles;
    city.accept_constructed_tower(4, 999, 2);
    city.update(750, {}, save);
    move_to(city, save, -1, 4);
    assert(city.snapshot().placement_valid);
    press(city, save, tb::Key::A);
    const tb::BuildCityUpdateResult discarded = city.update(3001, {}, save);
    assert(! discarded.save_dirty);
    assert(! discarded.placement_committed);
    assert(discarded.committed_total_population == 0);
    for(int index = 0; index < 25; ++index)
    {
        assert(save.city_tiles[index].type == city_before_discard[index].type);
        assert(save.city_tiles[index].population == city_before_discard[index].population);
        assert(save.city_tiles[index].roof == city_before_discard[index].roof);
    }
    assert(city.snapshot().total_population == 2200);
    std::cout << "DISCARD " << city.snapshot().total_population << ' ' << (discarded.save_dirty ? 1 : 0) << '\n';

    return 0;
}
