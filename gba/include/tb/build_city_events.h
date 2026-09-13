#ifndef TB_BUILD_CITY_EVENTS_H
#define TB_BUILD_CITY_EVENTS_H

#include <array>
#include <cstdint>

#include "tb/save_data.h"

namespace tb
{
struct BuildCityEventDefinition
{
    int id = -1;
    int localization_index = -1;
};

enum class BuildCityEventArgumentKind : uint8_t
{
    None = 0,
    Number,
    Text,
    TextPair,
};

struct BuildCityEvent
{
    int id = -1;
    int localization_index = -1;
    BuildCityEventArgumentKind argument_kind = BuildCityEventArgumentKind::None;
    int number_value = 0;
    int text_index0 = -1;
    int text_index1 = -1;
};

struct BuildCityProgressState
{
    int total_population = 0;
    int milestone = 0;
    int city_level = 0;
    int max_unlocked_building_type = 1;
    int max_trophy_building_type = 0;
    int occupied_tiles = 0;
};

[[nodiscard]] BuildCityEventDefinition build_city_event_definition(int id);

class BuildCityEventController
{
public:
    void clear_runtime();
    void on_city_entered(const BuildCityProgressState& state, const SaveData& save);
    void on_constructed_tower_accepted(const BuildCityProgressState& state, const SaveData& save);
    void on_placement_committed(
            const BuildCityProgressState& before,
            const BuildCityProgressState& after,
            const SaveData& save);

    [[nodiscard]] bool has_event() const;
    [[nodiscard]] int pending_count() const;
    [[nodiscard]] const BuildCityEvent& current_event() const;
    bool acknowledge(SaveData& save);

private:
    void _enqueue(int id, const SaveData& save);
    void _enqueue_number(int id, int value, const SaveData& save);
    void _enqueue_text(int id, int text_index, const SaveData& save);
    void _enqueue_text_pair(int id, int text_index0, int text_index1, const SaveData& save);
    void _enqueue_milestone(int milestone, const SaveData& save);

    std::array<BuildCityEvent, 46> _queue{};
    std::array<bool, 46> _queued{};
    int _head = 0;
    int _count = 0;
};
}

#endif
