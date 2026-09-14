#include <cassert>
#include <cstring>
#include <iostream>
#include "tb/hall_of_fame.h"

int main()
{
    tb::HallOfFameData hall{};
    tb::reset_hall_of_fame(hall);
    for(const auto& table : hall.tables)
    {
        for(const auto& entry : table)
        {
            assert(std::strcmp(entry.name.data(), "SUMEA") == 0);
            assert(entry.score == 0);
        }
    }
    assert(! tb::build_city_player_registered(hall));
    assert(! tb::qualify_hall_score(hall, tb::HallTable::QuickGame, 0).qualifies);

    auto q = tb::insert_hall_score(hall, tb::HallTable::QuickGame, 100, "mike!");
    assert(q.qualifies && q.position == 1);
    assert(std::strcmp(hall.tables[1][0].name.data(), "MIKE") == 0);
    tb::insert_hall_score(hall, tb::HallTable::QuickGame, 90, "B");
    tb::insert_hall_score(hall, tb::HallTable::QuickGame, 80, "C");
    assert(! tb::qualify_hall_score(hall, tb::HallTable::QuickGame, 80).qualifies);
    q = tb::insert_hall_score(hall, tb::HallTable::QuickGame, 90, "D");
    assert(q.qualifies && q.position == 3);
    assert(std::strcmp(hall.tables[1][1].name.data(), "B") == 0);
    assert(std::strcmp(hall.tables[1][2].name.data(), "D") == 0);

    // A named Build City score becomes the one remembered local-player entry.
    tb::reset_hall_of_fame(hall);
    tb::insert_hall_score(hall, tb::HallTable::BuildCity, 6000, "AAA");
    tb::insert_hall_score(hall, tb::HallTable::BuildCity, 4000, "BBB");
    q = tb::insert_build_city_player_score(hall, 5000, "city-01.");
    assert(q.qualifies && q.position == 2);
    assert(tb::build_city_player_registered(hall));
    assert(std::strcmp(hall.tables[0][1].name.data(), "CITY-01.") == 0);

    // Later city submissions update/re-sort that entry instead of adding duplicates.
    assert(tb::update_build_city_player_score(hall, 7000));
    assert(std::strcmp(hall.tables[0][0].name.data(), "CITY-01.") == 0);
    assert(hall.tables[0][0].score == 7000);
    assert(std::strcmp(hall.tables[0][1].name.data(), "AAA") == 0);
    int player_copies = 0;
    for(const auto& entry : hall.tables[0])
    {
        if(std::strcmp(entry.name.data(), "CITY-01.") == 0)
        {
            ++player_copies;
        }
    }
    assert(player_copies == 1);

    // Hall-of-fame semantics retain the player's best city score on a lower replacement.
    assert(! tb::update_build_city_player_score(hall, 4500));
    assert(hall.tables[0][0].score == 7000);

    // Forgetting the city player leaves historical scores intact but requires a new name later.
    tb::forget_build_city_player(hall);
    assert(! tb::build_city_player_registered(hall));
    assert(std::strcmp(hall.tables[0][0].name.data(), "CITY-01.") == 0);
    assert(hall.tables[0][0].score == 7000);

    std::cout << "hall of fame ok\n";
}
