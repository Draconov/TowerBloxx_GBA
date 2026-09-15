#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>

#include "tb/app_state.h"
#include "tb/build_city.h"
#include "tb/hall_of_fame.h"
#include "tb/save_data.h"

namespace
{
tb::InputFrame fresh(tb::Key key)
{
    return tb::InputFrame{tb::key_mask(key), tb::key_mask(key)};
}

void press(tb::BuildCity& city, tb::SaveData& save, tb::Key key)
{
    city.update(16, fresh(key), save);
}

void move_to(tb::BuildCity& city, tb::SaveData& save, int column, int row)
{
    auto snapshot = city.snapshot();
    while(snapshot.cursor_row > row) { press(city, save, tb::Key::Up); snapshot = city.snapshot(); }
    while(snapshot.cursor_row < row) { press(city, save, tb::Key::Down); snapshot = city.snapshot(); }
    while(snapshot.cursor_column > column) { press(city, save, tb::Key::Left); snapshot = city.snapshot(); }
    while(snapshot.cursor_column < column) { press(city, save, tb::Key::Right); snapshot = city.snapshot(); }
}

tb::BuildCityUpdateResult place_center(tb::BuildCity& city, tb::SaveData& save, uint8_t type, int population)
{
    city.accept_constructed_tower(type, population, 1);
    city.update(750, {}, save);
    assert(city.snapshot().cursor_column == 2 && city.snapshot().cursor_row == 2);
    assert(city.snapshot().placement_valid);
    press(city, save, tb::Key::A);
    return city.update(3001, {}, save);
}

int degree(int index)
{
    const int row = index / 5;
    const int column = index % 5;
    return (row > 0) + (row < 4) + (column > 0) + (column < 4);
}

std::array<int, 4> neighbors(int index)
{
    std::array<int, 4> result{-1, -1, -1, -1};
    int count = 0;
    const int row = index / 5;
    const int column = index % 5;
    if(column > 0) result[count++] = index - 1;
    if(column < 4) result[count++] = index + 1;
    if(row > 0) result[count++] = index - 5;
    if(row < 4) result[count++] = index + 5;
    return result;
}
}

int main()
{
    // Every grid location: cardinal-neighbour capabilities must stay in bounds
    // and reach exactly the highest family supported by its available neighbours.
    for(int index = 0; index < 25; ++index)
    {
        const auto adjacent = neighbors(index);
        tb::SaveData save = tb::make_default_save();
        tb::BuildCity empty(save);
        assert(empty.placement_capability(save, index) == 0);

        save.city_tiles[adjacent[0]].type = 1;
        tb::BuildCity with_blue(save);
        assert(with_blue.placement_capability(save, index) == 1);

        if(degree(index) >= 2)
        {
            save.city_tiles[adjacent[1]].type = 2;
            tb::BuildCity with_blue_red(save);
            assert(with_blue_red.placement_capability(save, index) == 2);
        }
        if(degree(index) >= 3)
        {
            save.city_tiles[adjacent[2]].type = 3;
            tb::BuildCity with_all(save);
            assert(with_all.placement_capability(save, index) == 3);
        }
    }

    // Every existing-family -> new-family replacement pair at a fully capable
    // centre cell, including lower-population replacements.
    for(uint8_t old_type = 1; old_type <= 4; ++old_type)
    {
        for(uint8_t new_type = 1; new_type <= 4; ++new_type)
        {
            tb::SaveData save = tb::make_default_save();
            save.city_tiles[12] = {1000, old_type, 1, 0, 0};
            save.city_tiles[11] = {100, 1, 1, 0, 0};
            save.city_tiles[13] = {100, 2, 1, 0, 0};
            save.city_tiles[7] = {100, 3, 1, 0, 0};
            tb::BuildCity city(save);
            const int before = city.snapshot().total_population;
            const int replacement_population = 100 + int(new_type) * 10;
            const auto result = place_center(city, save, new_type, replacement_population);
            assert(result.save_dirty && result.placement_committed);
            assert(save.city_tiles[12].type == new_type);
            assert(save.city_tiles[12].population == replacement_population);
            assert(city.snapshot().total_population == before - 1000 + replacement_population);
        }
    }

    // Every source milestone boundary, including unlock/trophy/city-level tiers.
    constexpr std::array<int, 21> thresholds = {
        0, 75, 150, 250, 400, 600, 800, 1000, 1400, 1800, 2200,
        3000, 4000, 5000, 6500, 8000, 9500, 11500, 14000, 17000, 19000,
    };
    constexpr std::array<int, 21> expected_unlock = {
        1,1,1,2,2,2,3,3,3,3,4,4,4,4,4,4,4,4,4,4,4,
    };
    constexpr std::array<int, 21> expected_trophy = {
        0,0,0,0,0,0,0,0,1,1,1,1,2,2,3,3,4,4,4,4,4,
    };
    constexpr std::array<int, 21> expected_level = {
        0,1,1,1,2,2,2,3,3,4,4,5,5,6,6,7,7,7,8,8,9,
    };
    for(int milestone = 0; milestone < int(thresholds.size()); ++milestone)
    {
        tb::SaveData save = tb::make_default_save();
        if(thresholds[milestone] > 0)
        {
            save.city_tiles[0] = {thresholds[milestone], 1, 1, 0, 0};
        }
        tb::BuildCity city(save);
        const auto snapshot = city.snapshot();
        assert(snapshot.milestone == milestone);
        assert(snapshot.max_unlocked_building_type == expected_unlock[milestone]);
        assert(snapshot.max_trophy_building_type == expected_trophy[milestone]);
        assert(snapshot.city_level == expected_level[milestone]);
    }

    // A completely full grid remains representable and counts all 25 cells.
    tb::SaveData full = tb::make_default_save();
    for(int index = 0; index < 25; ++index)
    {
        full.city_tiles[index] = {800 + index, uint8_t((index % 4) + 1), uint8_t(index % 3), 0, 0};
    }
    tb::BuildCity full_city(full);
    assert(full_city.snapshot().occupied_tiles == 25);
    int full_population = 0;
    for(const auto& tile : full.city_tiles) full_population += tile.population;
    assert(full_city.snapshot().total_population == full_population);
    assert(full_city.snapshot().milestone == 20);
    assert(full_city.snapshot().city_level == 9);

    // The left demolition/discard zone discards the newly constructed tower;
    // it must never erase an existing map cell.
    const auto before_discard = full.city_tiles;
    full_city.accept_constructed_tower(1, 50, 0);
    full_city.update(750, {}, full);
    move_to(full_city, full, -1, 4);
    assert(full_city.snapshot().placement_valid);
    press(full_city, full, tb::Key::A);
    const auto discarded = full_city.update(3001, {}, full);
    assert(! discarded.save_dirty && ! discarded.placement_committed);
    for(int index = 0; index < 25; ++index)
    {
        const auto& actual = full.city_tiles[index];
        const auto& expected = before_discard[index];
        assert(actual.population == expected.population);
        assert(actual.type == expected.type);
        assert(actual.roof == expected.roof);
        assert(actual.reserved0 == expected.reserved0);
        assert(actual.reserved1 == expected.reserved1);
    }

    // Remembered Build City Hall entry updates in place, and Reset City
    // forgets the identity without deleting its historical score/name.
    tb::SaveData hall_save = tb::make_default_save();
    auto inserted = tb::insert_build_city_player_score(hall_save.hall_of_fame, 5000, "PLAYER");
    assert(inserted.qualifies);
    assert(tb::build_city_player_registered(hall_save.hall_of_fame));
    assert(tb::update_build_city_player_score(hall_save.hall_of_fame, 7000));
    int copies = 0;
    for(const auto& entry : hall_save.hall_of_fame.tables[0])
    {
        if(std::strcmp(entry.name.data(), "PLAYER") == 0) ++copies;
    }
    assert(copies == 1);
    tb::reset_city_progress(hall_save);
    assert(! tb::build_city_player_registered(hall_save.hall_of_fame));
    assert(std::strcmp(hall_save.hall_of_fame.tables[0][0].name.data(), "PLAYER") == 0);
    assert(hall_save.hall_of_fame.tables[0][0].score == 7000);

    tb::finalize_save(full);
    assert(tb::valid_save(full));

    std::cout << "build city torture ok\n";
    return 0;
}
