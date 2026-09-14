#include "tb/build_city_visuals.h"

#include <cassert>
#include <cstdint>
#include <iostream>

int main()
{
    using tb::build_city_placement_effect_frame;
    using tb::build_city_selector_slot_active;
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

    // m.t is a 500ms browser selector timer: orange for 250ms, gray for 250ms.
    assert(build_city_selector_slot_active(0));
    assert(build_city_selector_slot_active(249));
    assert(! build_city_selector_slot_active(250));
    assert(! build_city_selector_slot_active(499));
    assert(build_city_selector_slot_active(500));
    assert(! build_city_selector_slot_active(-1));

    // Resource 29 is not a looping destruction animation. Replacements use
    // all six frames during the first 750ms window; empty lots use only
    // frames 2..5 during the later 2250..1500ms timer window.
    assert(build_city_placement_effect_frame(true, 3000) == -1);
    assert(build_city_placement_effect_frame(true, 2999) == 0);
    assert(build_city_placement_effect_frame(true, 2250) == 5);
    assert(build_city_placement_effect_frame(true, 2249) == -1);

    assert(build_city_placement_effect_frame(false, 2250) == -1);
    assert(build_city_placement_effect_frame(false, 2249) == 2);
    assert(build_city_placement_effect_frame(false, 1500) == 5);
    assert(build_city_placement_effect_frame(false, 1499) == -1);
    for(int timer = 1500; timer < 2250; ++timer)
    {
        const int frame = build_city_placement_effect_frame(false, timer);
        assert(frame >= 2 && frame <= 5);
    }

    std::cout << "build city visuals ok\n";
}
