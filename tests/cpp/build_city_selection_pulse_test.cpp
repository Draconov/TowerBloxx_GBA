#include <cassert>
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
}

int main()
{
    tb::SaveData save = tb::make_default_save();
    tb::BuildCity city(save);

    // A on an unlocked browser item starts the source 500ms confirmation pulse
    // instead of handing off to construction immediately.
    auto first = city.update(16, fresh(tb::Key::A), save);
    assert(! first.exit);
    assert(! city.construction_request().pending);
    assert(city.snapshot().construction_select_ms == 500);

    city.update(249, {}, save);
    assert(city.snapshot().construction_select_ms == 251);
    assert(! city.construction_request().pending);

    // The selected building moves +2/-2 during the final <250ms phase.
    city.update(2, {}, save);
    assert(city.snapshot().construction_select_ms == 249);
    assert(! city.construction_request().pending);

    city.update(248, {}, save);
    assert(city.snapshot().construction_select_ms == 1);
    assert(! city.construction_request().pending);

    city.update(1, {}, save);
    assert(city.snapshot().construction_select_ms == 0);
    assert(city.construction_request().pending);
    assert(city.construction_request().building_type == 1);

    std::cout << "build city selection pulse ok\n";
}
