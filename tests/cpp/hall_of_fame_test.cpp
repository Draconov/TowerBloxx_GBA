#include <cassert>
#include <cstring>
#include <iostream>
#include "tb/hall_of_fame.h"
int main()
{
    tb::HallOfFameData hall{}; tb::reset_hall_of_fame(hall);
    for(const auto& table : hall.tables) for(const auto& entry : table) { assert(std::strcmp(entry.name.data(), "SUMEA") == 0); assert(entry.score == 0); }
    assert(! tb::qualify_hall_score(hall, tb::HallTable::QuickGame, 0).qualifies);
    auto q = tb::insert_hall_score(hall, tb::HallTable::QuickGame, 100, "mike!"); assert(q.qualifies && q.position == 1); assert(std::strcmp(hall.tables[1][0].name.data(), "MIKE") == 0);
    tb::insert_hall_score(hall, tb::HallTable::QuickGame, 90, "B"); tb::insert_hall_score(hall, tb::HallTable::QuickGame, 80, "C"); assert(! tb::qualify_hall_score(hall, tb::HallTable::QuickGame, 80).qualifies); q = tb::insert_hall_score(hall, tb::HallTable::QuickGame, 90, "D"); assert(q.qualifies && q.position == 3); assert(std::strcmp(hall.tables[1][1].name.data(), "B") == 0); assert(std::strcmp(hall.tables[1][2].name.data(), "D") == 0);
    q = tb::insert_hall_score(hall, tb::HallTable::BuildCity, 5000, "CITY-01."); assert(q.qualifies && q.position == 1); assert(std::strcmp(hall.tables[0][0].name.data(), "CITY-01.") == 0);
    q = tb::insert_hall_score(hall, tb::HallTable::BuildCity, 6000, "abcdefghijkl"); assert(q.qualifies && q.position == 1); assert(std::strcmp(hall.tables[0][0].name.data(), "ABCDEFGH") == 0);
    q = tb::insert_hall_score(hall, tb::HallTable::BuildCity, 7000, ""); assert(q.qualifies && q.position == 1); assert(std::strcmp(hall.tables[0][0].name.data(), "SUMEA") == 0);
    std::cout << "hall of fame ok\n";
}
