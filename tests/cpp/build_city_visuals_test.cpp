#include "tb/build_city_visuals.h"

#include <cassert>
#include <cstdint>
#include <iostream>

int main()
{
    using tb::build_city_placement_effect_frame;
    using tb::build_city_discard_effect_frame;
    using tb::build_city_preview_raised;
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

    // GBA UX adaptation: an unlocked highlighted browser tower stays raised
    // by +2px right / -2px up for as long as its row is selected. Locked
    // rows keep the selector pulse but do not show/raise a tower.
    assert(build_city_preview_raised(1, 1, 1));
    assert(build_city_preview_raised(2, 2, 4));
    assert(! build_city_preview_raised(1, 2, 4));
    assert(! build_city_preview_raised(2, 2, 1));
    assert(! build_city_preview_raised(0, 0, 4));
    assert(! build_city_preview_raised(5, 5, 4));

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

    // The bulldozer/discard slot must use the same six-frame destruction
    // sequence as replacing an occupied regular city cell.
    for(int timer = -50; timer <= 3050; timer += 25)
    {
        assert(build_city_discard_effect_frame(timer) ==
               build_city_placement_effect_frame(true, timer));
    }


    // Source m.class q/r/s population roll: the changed suffix length is
    // determined by the highest decimal place whose quotient changed.
    assert(tb::build_city_changed_population_cells(12345, 12346) == 1);
    assert(tb::build_city_changed_population_cells(12349, 12350) == 2);
    assert(tb::build_city_changed_population_cells(12999, 13000) == 4);
    assert(tb::build_city_changed_population_cells(9999, 10000) == 5);
    assert(tb::build_city_changed_population_cells(500, 500) == 0);

    tb::BuildCityPopulationRoll roll;
    roll.reset();
    roll.start(99, 100);
    assert(roll.animating());
    assert(roll.changed_cells() == 3);
    assert(roll.panel_state_for_cell(1) == 0);
    assert(roll.panel_state_for_cell(2) == 0);
    assert(roll.panel_state_for_cell(3) == 0);
    assert(roll.panel_state_for_cell(4) == 0);
    assert(roll.panel_state_for_cell(5) == 3);
    roll.update(16);
    assert(roll.panel_state() == 1);
    assert(roll.panel_state_for_cell(2) == 1);
    assert(roll.panel_state_for_cell(3) == 1);
    assert(roll.panel_state_for_cell(4) == 1);
    roll.update(16);
    assert(roll.panel_state() == 2);
    roll.update(269); // timer crosses below zero: one rolling cell completes.
    assert(roll.changed_cells() == 2);
    assert(roll.timer_ms() == 300);

    // Decreases use the red digit strip only during the 1199ms tail, blinking
    // 200ms on / 200ms off exactly like m.a(Graphics, boolean).
    roll.reset();
    roll.start(1000, 999);
    assert(roll.decreasing());
    // Four changed cells, one 300ms phase each.
    for(int cell = 0; cell < 4; ++cell)
    {
        roll.update(301);
    }
    assert(roll.changed_cells() == 0);
    assert(roll.timer_ms() == 1199);
    assert(! roll.use_red_digits()); // 1199 % 400 = 399.
    roll.update(200);                 // 999 % 400 = 199.
    assert(roll.use_red_digits());
    roll.update(200);                 // 799 % 400 = 399.
    assert(! roll.use_red_digits());
    roll.update(800);
    assert(! roll.animating());

    std::cout << "build city visuals ok\n";
}
