#include "tb/build_city_events.h"

#include <array>
#include <cassert>

namespace tb
{
namespace
{
constexpr std::array<int, 46> localization_indices = {
    36, 37, 38, 39, 40, 41,
    43, 43, 43, 58, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 59, 43, 60,
    45, 44, 44, 44, 44, 44, 44, 44, 44,
    42, 47, 49, 50, 48, 46,
    51, 51, 51, 51,
    52, 53, 54,
};

constexpr std::array<int, 21> milestone_populations = {
    0, 75, 150, 250, 400, 600, 800, 1000, 1400, 1800, 2200,
    3000, 4000, 5000, 6500, 8000, 9500, 11500, 14000, 17000, 19000,
};

constexpr std::array<int, 10> city_level_milestones = {0, 1, 4, 7, 9, 11, 13, 15, 18, 20};
constexpr std::array<int, 4> building_unlock_milestones = {0, 3, 6, 10};
constexpr std::array<int, 4> trophy_unlock_milestones = {8, 12, 14, 16};

int city_level_for_milestone(int milestone)
{
    int level = 0;
    for(int index = 1; index < int(city_level_milestones.size()); ++index)
    {
        if(milestone >= city_level_milestones[index])
        {
            level = index;
        }
    }
    return level;
}
}

BuildCityEventDefinition build_city_event_definition(int id)
{
    assert(id >= 0 && id < int(localization_indices.size()));
    return BuildCityEventDefinition{id, localization_indices[id]};
}

void BuildCityEventController::clear_runtime()
{
    _queue = {};
    _queued = {};
    _head = 0;
    _count = 0;
}

void BuildCityEventController::on_city_entered(const BuildCityProgressState& state, const SaveData& save)
{
    if(state.total_population == 0 && state.occupied_tiles == 0)
    {
        _enqueue(0, save);
        _enqueue(1, save);
        _enqueue(2, save);
        return;
    }

    // Fix 10 reserved these flags but never dispatched them. An all-zero progressed
    // city is therefore treated as a legacy city and is not flooded on first entry.
    // Once any Fix 11 event has been acknowledged, reconstruct all still-unseen
    // state-derived events after an interrupted/powered-off modal chain.
    bool tutorial_started = false;
    for(uint8_t flag : save.city_tutorial_flags)
    {
        if(flag != 0)
        {
            tutorial_started = true;
            break;
        }
    }
    if(tutorial_started)
    {
        BuildCityProgressState baseline{};
        on_placement_committed(baseline, state, save);
    }
}

void BuildCityEventController::on_constructed_tower_accepted(const BuildCityProgressState& state, const SaveData& save)
{
    if(state.total_population == 0 && state.occupied_tiles == 0)
    {
        _enqueue(3, save);
    }
    if(state.milestone == 5)
    {
        _enqueue(38, save);
    }
}

void BuildCityEventController::on_placement_committed(
        const BuildCityProgressState& before,
        const BuildCityProgressState& after,
        const SaveData& save)
{
    if(before.occupied_tiles == 0 && after.occupied_tiles > 0)
    {
        _enqueue(4, save);
    }

    int first_milestone = before.milestone + 1;
    if(first_milestone < 1)
    {
        first_milestone = 1;
    }
    int last_milestone = after.milestone;
    if(last_milestone > 20)
    {
        last_milestone = 20;
    }

    for(int milestone = first_milestone; milestone <= last_milestone; ++milestone)
    {
        _enqueue_milestone(milestone, save);

        const int level = city_level_for_milestone(milestone);
        if(level > 0 && city_level_milestones[level] == milestone)
        {
            if(level == 1)
            {
                _enqueue(24, save);
            }
            else
            {
                _enqueue_text_pair(23 + level, 69 + level, 70 + level, save);
            }
        }

        if(milestone == 2)
        {
            _enqueue(33, save);
        }

        for(int unlock_index = 1; unlock_index < int(building_unlock_milestones.size()); ++unlock_index)
        {
            if(building_unlock_milestones[unlock_index] == milestone)
            {
                _enqueue(33 + unlock_index, save);
                if(unlock_index == 1)
                {
                    _enqueue(37, save);
                }
            }
        }

        for(int trophy_index = 0; trophy_index < int(trophy_unlock_milestones.size()); ++trophy_index)
        {
            if(trophy_unlock_milestones[trophy_index] == milestone)
            {
                _enqueue_text(39 + trophy_index, 87 + trophy_index, save);
            }
        }
    }

    if(after.occupied_tiles == 25 && before.occupied_tiles < 25)
    {
        const int city_name_index = after.city_level > 0 ? 70 + after.city_level : -1;
        _enqueue_text(44, city_name_index, save);
        _enqueue_number(45, milestone_populations[20], save);
    }
}

bool BuildCityEventController::has_event() const
{
    return _count > 0;
}

int BuildCityEventController::pending_count() const
{
    return _count;
}

const BuildCityEvent& BuildCityEventController::current_event() const
{
    assert(_count > 0);
    return _queue[_head];
}

bool BuildCityEventController::acknowledge(SaveData& save)
{
    if(_count <= 0)
    {
        return false;
    }

    const int id = _queue[_head].id;
    assert(id >= 0 && id < int(save.city_tutorial_flags.size()));
    const bool changed = save.city_tutorial_flags[id] == 0;
    save.city_tutorial_flags[id] = 1;
    _queued[id] = false;
    _head = (_head + 1) % int(_queue.size());
    --_count;
    return changed;
}

void BuildCityEventController::_enqueue(int id, const SaveData& save)
{
    if(id < 0 || id >= int(_queued.size()) || save.city_tutorial_flags[id] != 0 || _queued[id])
    {
        return;
    }
    assert(_count < int(_queue.size()));
    const int tail = (_head + _count) % int(_queue.size());
    const BuildCityEventDefinition definition = build_city_event_definition(id);
    _queue[tail] = BuildCityEvent{definition.id, definition.localization_index};
    _queued[id] = true;
    ++_count;
}

void BuildCityEventController::_enqueue_number(int id, int value, const SaveData& save)
{
    const int before = _count;
    _enqueue(id, save);
    if(_count != before)
    {
        const int tail = (_head + _count - 1) % int(_queue.size());
        _queue[tail].argument_kind = BuildCityEventArgumentKind::Number;
        _queue[tail].number_value = value;
    }
}

void BuildCityEventController::_enqueue_text(int id, int text_index, const SaveData& save)
{
    const int before = _count;
    _enqueue(id, save);
    if(_count != before)
    {
        const int tail = (_head + _count - 1) % int(_queue.size());
        _queue[tail].argument_kind = BuildCityEventArgumentKind::Text;
        _queue[tail].text_index0 = text_index;
    }
}

void BuildCityEventController::_enqueue_text_pair(int id, int text_index0, int text_index1, const SaveData& save)
{
    const int before = _count;
    _enqueue(id, save);
    if(_count != before)
    {
        const int tail = (_head + _count - 1) % int(_queue.size());
        _queue[tail].argument_kind = BuildCityEventArgumentKind::TextPair;
        _queue[tail].text_index0 = text_index0;
        _queue[tail].text_index1 = text_index1;
    }
}

void BuildCityEventController::_enqueue_milestone(int milestone, const SaveData& save)
{
    if(milestone == 1)
    {
        _enqueue_number(5, milestone_populations[1], save);
    }
    else if(milestone >= 2 && milestone <= 19)
    {
        _enqueue_number(4 + milestone, milestone_populations[milestone], save);
    }
    else if(milestone == 20)
    {
        _enqueue(43, save);
    }
}
}
