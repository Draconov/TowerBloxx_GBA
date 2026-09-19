#include "tb/build_city.h"

#include <array>
#include <cassert>

namespace tb
{
namespace
{
constexpr std::array<int, 21> milestones = {
    0, 75, 150, 250, 400, 600, 800, 1000, 1400, 1800, 2200,
    3000, 4000, 5000, 6500, 8000, 9500, 11500, 14000, 17000, 19000,
};
constexpr std::array<int, 4> building_unlock_milestones = {0, 3, 6, 10};
constexpr std::array<int, 4> trophy_unlock_milestones = {8, 12, 14, 16};
constexpr std::array<int, 10> city_level_milestones = {0, 1, 4, 7, 9, 11, 13, 15, 18, 20};
constexpr std::array<int, 4> target_heights = {10, 20, 30, 40};

int largest_threshold_index(const auto& values, int value)
{
    int result = 0;
    for(int index = 0; index < int(values.size()); ++index)
    {
        if(value >= values[index])
        {
            result = index;
        }
        else
        {
            break;
        }
    }
    return result;
}
}

BuildCity::BuildCity(const SaveData& save)
{
    reset_from_save(save);
}

void BuildCity::reset_from_save(const SaveData& save)
{
    _mode = BuildCityMode::Browse;
    _sandbox_active = false;
    _secret_step = 0;
    _cursor_column = 2;
    _cursor_row = 2;
    _placement_committing = false;
    _placement_timer_ms = -1;
    _placement_transition_ms = 0;
    _replacement_population = 0;
    _pending_type = 0;
    _pending_population = 0;
    _pending_roof = 0;
    _last_population_delta = 0;
    _occupied_tiles = 0;
    _placement_transition_ms = 0;
    _replacement_population = 0;
    _saved_populations.fill(0);
    _request = {};
    _selected_type = 1;
    _construction_select_ms = 0;
    _max_unlocked_type = 1;
    _max_trophy_type = 0;
    _recalculate_progress(save, true);
    _refresh_capabilities(save);
}

BuildCityUpdateResult BuildCity::update(int delta_ms, const InputFrame& input, SaveData& save)
{
    BuildCityUpdateResult result;
    if(delta_ms < 0)
    {
        delta_ms = 0;
    }

    if(_mode == BuildCityMode::Browse && input.held(Key::Select) && _construction_select_ms == 0)
    {
        // Hold SELECT and press Up, Up, Down, Down to enter sandbox mode.
        // SELECT prevents ordinary browse navigation while entering the code.
        constexpr std::array<Key, 4> secret = {
            Key::Up, Key::Up, Key::Down, Key::Down,
        };
        for(Key key : {Key::Up, Key::Down, Key::Left, Key::Right})
        {
            if(input.pressed(key))
            {
                _secret_step = key == secret[_secret_step] ? _secret_step + 1 :
                        int(key == secret[0]);
                if(_secret_step == int(secret.size()))
                {
                    _secret_step = 0;
                    if(! _sandbox_active)
                    {
                        _sandbox_active = true;
                        _max_unlocked_type = 4;
                        _max_trophy_type = 4;
                        _refresh_capabilities(save);
                        result.sandbox_activated = true;
                    }
                }
            }
        }
        return result;
    }
    if(! input.held(Key::Select))
    {
        _secret_step = 0;
    }

    if(_mode == BuildCityMode::Browse)
    {
        if(_construction_select_ms > 0)
        {
            _construction_select_ms -= delta_ms;
            if(_construction_select_ms <= 0)
            {
                _construction_select_ms = 0;
                _request.pending = true;
                _request.building_type = uint8_t(_selected_type);
                _request.target_height = target_heights[_selected_type - 1];
                _request.trophy_eligible = _selected_type <= _max_trophy_type;
                _request.stationary_crane = _sandbox_active;
            }
            return result;
        }

        if(input.pressed(Key::Up) && _selected_type > 1)
        {
            --_selected_type;
        }
        else if(input.pressed(Key::Down) && _selected_type < 4)
        {
            ++_selected_type;
        }
        else if(input.pressed(Key::A))
        {
            if(_selected_type <= _max_unlocked_type)
            {
                _construction_select_ms = 500;
            }
        }
        else if(input.pressed(Key::B))
        {
            result.exit = true;
        }
        return result;
    }

    if(_placement_transition_ms > 0)
    {
        _placement_transition_ms -= delta_ms;
        if(_placement_transition_ms < 0)
        {
            _placement_transition_ms = 0;
        }
        return result;
    }

    if(_placement_committing)
    {
        _placement_timer_ms -= delta_ms;
        if(_placement_timer_ms < 0)
        {
            return _finish_placement(save);
        }
        return result;
    }

    if(input.pressed(Key::Up))
    {
        if(_cursor_column >= 0 && _cursor_row > 0)
        {
            --_cursor_row;
        }
    }
    else if(input.pressed(Key::Down))
    {
        if(_cursor_row < 4)
        {
            ++_cursor_row;
        }
    }
    else if(input.pressed(Key::Left))
    {
        if(_cursor_column > 0)
        {
            --_cursor_column;
        }
        else if(_cursor_column == 0)
        {
            _cursor_column = -1;
            _cursor_row = 4;
        }
    }
    else if(input.pressed(Key::Right))
    {
        if(_cursor_column < 0)
        {
            _cursor_column = 0;
        }
        else if(_cursor_column < 4)
        {
            ++_cursor_column;
        }
    }
    else if(input.pressed(Key::A) && _placement_valid())
    {
        _start_placement_commit(save);
    }

    if(_cursor_column >= 0)
    {
        _replacement_population = _saved_populations[_cursor_row * 5 + _cursor_column];
    }
    else
    {
        _replacement_population = 0;
    }

    return result;
}

BuildCitySnapshot BuildCity::snapshot() const
{
    BuildCitySnapshot result;
    result.mode = _mode;
    result.sandbox_active = _sandbox_active;
    result.total_population = _total_population;
    result.milestone = _milestone;
    result.city_level = _city_level;
    result.max_unlocked_building_type = _max_unlocked_type;
    result.max_trophy_building_type = _max_trophy_type;
    result.selected_building_type = _selected_type;
    result.selected_unlock_population = milestones[building_unlock_milestones[_selected_type - 1]];
    result.construction_select_ms = _construction_select_ms;
    result.cursor_column = _cursor_column;
    result.cursor_row = _cursor_row;
    result.placement_committing = _placement_committing;
    result.placement_timer_ms = _placement_timer_ms;
    result.pending_building_type = _pending_type;
    result.pending_population = _pending_population;
    result.pending_roof = _pending_roof;
    result.last_population_delta = _last_population_delta;
    result.occupied_tiles = _occupied_tiles;
    result.current_milestone_population = milestones[_milestone];
    result.next_milestone_population = milestones[_milestone < int(milestones.size()) - 1 ? _milestone + 1 : _milestone];
    result.placement_transition_ms = _placement_transition_ms;
    result.replacement_population = _replacement_population;
    result.placement_capabilities = _placement_capabilities;
    result.placement_valid = _placement_valid();
    return result;
}

bool BuildCity::sandbox_active() const
{
    return _sandbox_active;
}

BuildCityConstructionRequest BuildCity::construction_request() const
{
    return _request;
}

void BuildCity::clear_construction_request()
{
    _request = {};
}

void BuildCity::accept_constructed_tower(uint8_t building_type, int population, uint8_t roof)
{
    assert(building_type >= 1 && building_type <= 4);
    assert(population >= 0);
    assert(roof <= 2);
    _request = {};
    _construction_select_ms = 0;
    _mode = BuildCityMode::Placement;
    _cursor_column = 2;
    _cursor_row = 2;
    _placement_committing = false;
    _placement_timer_ms = -1;
    _placement_transition_ms = 750;
    _replacement_population = _saved_populations[12];
    _pending_type = building_type;
    _pending_population = population;
    _pending_roof = roof;
    _last_population_delta = 0;
}

const CityTileSave& BuildCity::tile(const SaveData& save, int index) const
{
    assert(index >= 0 && index < 25);
    return save.city_tiles[index];
}

int BuildCity::placement_capability(const SaveData& save, int index) const
{
    assert(index >= 0 && index < 25);
    const int row = index / 5;
    const int column = index % 5;
    bool has_type1 = false;
    bool has_type2 = false;
    bool has_type3 = false;

    const auto inspect = [&](int neighbor)
    {
        const uint8_t type = save.city_tiles[neighbor].type;
        if(type == 1) { has_type1 = true; }
        else if(type == 2) { has_type2 = true; }
        else if(type == 3) { has_type3 = true; }
    };

    if(column > 0) { inspect(index - 1); }
    if(column < 4) { inspect(index + 1); }
    if(row > 0) { inspect(index - 5); }
    if(row < 4) { inspect(index + 5); }

    if(has_type1 && has_type2 && has_type3) { return 3; }
    if(has_type1 && has_type2) { return 2; }
    if(has_type1) { return 1; }
    return 0;
}

void BuildCity::_recalculate_progress(const SaveData& save, bool select_new_unlock)
{
    int total = 0;
    int occupied = 0;
    for(const CityTileSave& tile : save.city_tiles)
    {
        total += tile.population;
        if(tile.type != 0)
        {
            ++occupied;
        }
    }
    _total_population = total;
    _occupied_tiles = occupied;
    _milestone = largest_threshold_index(milestones, total);

    int max_unlocked_index = 0;
    for(int index = 0; index < int(building_unlock_milestones.size()); ++index)
    {
        if(_milestone >= building_unlock_milestones[index])
        {
            max_unlocked_index = index;
        }
    }
    const int previous_max = _max_unlocked_type;
    _max_unlocked_type = _sandbox_active ? 4 : max_unlocked_index + 1;
    if(select_new_unlock && _max_unlocked_type != previous_max)
    {
        _selected_type = _max_unlocked_type;
    }

    _max_trophy_type = 0;
    for(int index = 0; index < int(trophy_unlock_milestones.size()); ++index)
    {
        if(_milestone >= trophy_unlock_milestones[index])
        {
            _max_trophy_type = index + 1;
        }
    }

    if(_sandbox_active)
    {
        _max_trophy_type = 4;
    }

    _city_level = 0;
    for(int index = 0; index < int(city_level_milestones.size()); ++index)
    {
        if(_milestone >= city_level_milestones[index])
        {
            _city_level = index;
        }
    }
}

void BuildCity::_refresh_capabilities(const SaveData& save)
{
    for(int index = 0; index < 25; ++index)
    {
        _placement_capabilities[index] = _sandbox_active ? 3 : uint8_t(placement_capability(save, index));
        _saved_populations[index] = save.city_tiles[index].population;
    }
    if(_mode == BuildCityMode::Placement && _cursor_column >= 0)
    {
        _replacement_population = _saved_populations[_cursor_row * 5 + _cursor_column];
    }
    else
    {
        _replacement_population = 0;
    }
}

bool BuildCity::_placement_valid() const
{
    if(_mode != BuildCityMode::Placement || _placement_committing || _placement_transition_ms > 0)
    {
        return false;
    }
    if(_cursor_column < 0)
    {
        return true;
    }
    const int index = _cursor_row * 5 + _cursor_column;
    return _sandbox_active || int(_placement_capabilities[index]) >= int(_pending_type) - 1;
}

void BuildCity::_start_placement_commit(const SaveData& save)
{
    _placement_committing = true;
    _placement_timer_ms = 3000;
    _last_population_delta = 0;
    if(_cursor_column >= 0)
    {
        const int index = _cursor_row * 5 + _cursor_column;
        _last_population_delta = _pending_population - save.city_tiles[index].population;
        _total_population += _last_population_delta;
    }
}

BuildCityUpdateResult BuildCity::_finish_placement(SaveData& save)
{
    BuildCityUpdateResult result;
    if(_cursor_column >= 0)
    {
        const int index = _cursor_row * 5 + _cursor_column;
        CityTileSave& tile = save.city_tiles[index];
        tile.type = _pending_type;
        tile.population = _pending_population;
        tile.roof = _pending_roof;
        result.save_dirty = true;
    }

    _mode = BuildCityMode::Browse;
    _placement_committing = false;
    _placement_timer_ms = -1;
    _pending_type = 0;
    _pending_population = 0;
    _pending_roof = 0;
    _cursor_column = 2;
    _cursor_row = 2;
    if(result.save_dirty)
    {
        _recalculate_progress(save, true);
        _refresh_capabilities(save);
        result.placement_committed = true;
        result.committed_total_population = _total_population;
    }
    return result;
}
}
