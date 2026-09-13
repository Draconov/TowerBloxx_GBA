#include "tb/build_city_visuals.h"

#include <cassert>
#include <cstdint>
#include <iostream>

int main()
{
    using tb::build_city_valid_lot_rgb;

    assert(build_city_valid_lot_rgb(1, 0) == 0x0054A0u);
    assert(build_city_valid_lot_rgb(1, 200) == 0x1E8ECFu);
    assert(build_city_valid_lot_rgb(1, 400) == 0x3CC8FFu);
    assert(build_city_valid_lot_rgb(1, 600) == 0x1E8ECFu);
    assert(build_city_valid_lot_rgb(1, 799) == 0x0054A0u);
    assert(build_city_valid_lot_rgb(1, 800) == 0x0054A0u);

    assert(build_city_valid_lot_rgb(2, 0) == 0xA00200u);
    assert(build_city_valid_lot_rgb(2, 400) == 0xFF6946u);
    assert(build_city_valid_lot_rgb(3, 0) == 0x009800u);
    assert(build_city_valid_lot_rgb(3, 400) == 0x37FF37u);
    assert(build_city_valid_lot_rgb(4, 0) == 0x935A00u);
    assert(build_city_valid_lot_rgb(4, 400) == 0xF0FF00u);

    // Clamp unknown building IDs to the nearest source family instead of
    // indexing outside the recovered four-entry endpoint table.
    assert(build_city_valid_lot_rgb(0, 0) == 0x0054A0u);
    assert(build_city_valid_lot_rgb(5, 400) == 0xF0FF00u);

    std::cout << "build city visuals ok\n";
}
