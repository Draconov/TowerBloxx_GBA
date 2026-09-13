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
    save.city_tiles[12].type = 1;
    save.city_tiles[12].population = 120;
    tb::finalize_save(save);

    tb::BuildCity city(save);
    tb::BuildCitySnapshot browse = city.snapshot();
    assert(browse.occupied_tiles == 1);
    assert(browse.next_milestone_population == 150);
    assert(browse.current_milestone_population == 75);

    city.accept_constructed_tower(1, 200, 1);
    tb::BuildCitySnapshot placement = city.snapshot();
    assert(placement.mode == tb::BuildCityMode::Placement);
    assert(placement.placement_transition_ms == 750);
    assert(placement.replacement_population == 120);

    // The source placement-entry slide blocks movement/placement until 750 ms expires.
    city.update(0, fresh(tb::Key::Right), save);
    assert(city.snapshot().cursor_column == 2);
    city.update(749, {}, save);
    assert(city.snapshot().placement_transition_ms == 1);
    city.update(1, fresh(tb::Key::Right), save);
    assert(city.snapshot().placement_transition_ms == 0);
    assert(city.snapshot().cursor_column == 2); // expiry frame itself still consumes transition time.
    city.update(16, fresh(tb::Key::Right), save);
    assert(city.snapshot().cursor_column == 3);
    assert(city.snapshot().replacement_population == 0);

    city.update(16, fresh(tb::Key::Left), save);
    assert(city.snapshot().cursor_column == 2);
    assert(city.snapshot().replacement_population == 120);

    // Capability data remains available in the snapshot for the renderer's all-valid-sector flashing pass.
    assert(city.snapshot().placement_capabilities[12] == city.placement_capability(save, 12));
    assert(city.snapshot().placement_capabilities[0] == city.placement_capability(save, 0));

    std::cout << "build city hud state ok\n";
}
