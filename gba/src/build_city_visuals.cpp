#include "tb/build_city_visuals.h"

namespace tb
{
uint32_t build_city_valid_lot_rgb(int building_type, int flash_ms)
{
    static constexpr uint32_t starts[4] = {
        0x0054A0u,
        0xA00200u,
        0x009800u,
        0x935A00u,
    };
    static constexpr uint32_t ends[4] = {
        0x3CC8FFu,
        0xFF6946u,
        0x37FF37u,
        0xF0FF00u,
    };

    if(building_type < 1)
    {
        building_type = 1;
    }
    else if(building_type > 4)
    {
        building_type = 4;
    }

    int phase_ms = flash_ms % 800;
    if(phase_ms < 0)
    {
        phase_ms += 800;
    }
    int phase = phase_ms - 400;
    if(phase > 0)
    {
        phase = -phase;
    }
    phase += 400;

    const uint32_t start = starts[building_type - 1];
    const uint32_t end = ends[building_type - 1];
    const int start_r = int((start >> 16) & 0xFFu);
    const int start_g = int((start >> 8) & 0xFFu);
    const int start_b = int(start & 0xFFu);
    const int end_r = int((end >> 16) & 0xFFu);
    const int end_g = int((end >> 8) & 0xFFu);
    const int end_b = int(end & 0xFFu);

    const uint32_t red = uint32_t(start_r + (end_r - start_r) * phase / 400);
    const uint32_t green = uint32_t(start_g + (end_g - start_g) * phase / 400);
    const uint32_t blue = uint32_t(start_b + (end_b - start_b) * phase / 400);
    return (red << 16) | (green << 8) | blue;
}

bool build_city_preview_raised(int type, int selected_type, int max_unlocked_type)
{
    return type >= 1 && type <= 4 && type == selected_type && type <= max_unlocked_type;
}

bool build_city_selector_slot_active(int flash_ms)
{
    int phase_ms = flash_ms % 500;
    if(phase_ms < 0)
    {
        phase_ms += 500;
    }
    return phase_ms < 250;
}

int build_city_placement_effect_frame(bool replacing, int placement_timer_ms)
{
    // m.a(Graphics, boolean): resource 29 is a bounded placement/replacement
    // transition, not a six-frame loop. Empty lots deliberately skip the
    // first two destruction-looking frames.
    const int local_timer = replacing ? placement_timer_ms - 2250 : placement_timer_ms - 1500;
    if(local_timer < 0 || local_timer >= 750)
    {
        return -1;
    }

    if(replacing)
    {
        return 5 - (6 * local_timer / 750);
    }
    return 5 - (4 * local_timer / 750);
}

int build_city_discard_effect_frame(int placement_timer_ms)
{
    // The bulldozer destroys the newly-built tower with the same resource-29
    // six-frame destruction pass used when an occupied city cell is replaced.
    return build_city_placement_effect_frame(true, placement_timer_ms);
}

int build_city_changed_population_cells(int old_population, int new_population)
{
    if(old_population / 10000 != new_population / 10000)
    {
        return 5;
    }
    if(old_population / 1000 != new_population / 1000)
    {
        return 4;
    }
    if(old_population / 100 != new_population / 100)
    {
        return 3;
    }
    if(old_population / 10 != new_population / 10)
    {
        return 2;
    }
    return old_population != new_population ? 1 : 0;
}

void BuildCityPopulationRoll::reset()
{
    _timer_ms = -1;
    _panel_state = 0;
    _changed_cells = 0;
    _decreasing = false;
}

void BuildCityPopulationRoll::start(int old_population, int new_population)
{
    _changed_cells = build_city_changed_population_cells(old_population, new_population);
    _panel_state = 0;
    _decreasing = new_population < old_population;
    _timer_ms = _changed_cells > 0 ? 300 : -1;
}

void BuildCityPopulationRoll::update(int delta_ms)
{
    if(_timer_ms < 0)
    {
        return;
    }
    if(delta_ms < 0)
    {
        delta_ms = 0;
    }

    _timer_ms -= delta_ms;
    if(_changed_cells > 0)
    {
        _panel_state = (_panel_state % 2) + 1;
        if(_timer_ms < 0)
        {
            --_changed_cells;
            _timer_ms = _changed_cells > 0 ? 300 : 1199;
        }
    }
    else if(_timer_ms < 0)
    {
        reset();
    }
}

bool BuildCityPopulationRoll::animating() const
{
    return _timer_ms >= 0;
}

bool BuildCityPopulationRoll::decreasing() const
{
    return _decreasing;
}

int BuildCityPopulationRoll::changed_cells() const
{
    return _changed_cells;
}

int BuildCityPopulationRoll::timer_ms() const
{
    return _timer_ms;
}

int BuildCityPopulationRoll::panel_state() const
{
    return _panel_state;
}

int BuildCityPopulationRoll::panel_state_for_cell(int cell_index) const
{
    if(cell_index == 5)
    {
        return 3;
    }
    if(cell_index < 0 || cell_index > 4)
    {
        return 0;
    }
    return 5 - cell_index <= _changed_cells ? _panel_state : 0;
}

bool BuildCityPopulationRoll::use_red_digits() const
{
    return _decreasing && _changed_cells == 0 && _timer_ms >= 0 && (_timer_ms % 400) < 200;
}

}
