#ifndef TB_BUILD_CITY_H
#define TB_BUILD_CITY_H

#include <array>
#include <cstdint>

#include "tb/app_state.h"
#include "tb/save_data.h"

namespace tb
{
enum class BuildCityMode : uint8_t
{
    Browse = 0,
    Placement,
};

struct BuildCityConstructionRequest
{
    bool pending = false;
    uint8_t building_type = 0;
    int target_height = 0;
    bool trophy_eligible = false;
};

struct BuildCityUpdateResult
{
    bool exit = false;
    bool save_dirty = false;
    bool placement_committed = false;
    int committed_total_population = 0;
};

struct BuildCitySnapshot
{
    BuildCityMode mode = BuildCityMode::Browse;
    int total_population = 0;
    int milestone = 0;
    int city_level = 0;
    int max_unlocked_building_type = 1;
    int max_trophy_building_type = 0;
    int selected_building_type = 1;
    int selected_unlock_population = 0;
    int construction_select_ms = 0;
    int cursor_column = 2;
    int cursor_row = 2;
    bool placement_valid = false;
    bool placement_committing = false;
    int placement_timer_ms = -1;
    uint8_t pending_building_type = 0;
    int pending_population = 0;
    uint8_t pending_roof = 0;
    int last_population_delta = 0;
    int occupied_tiles = 0;
    int current_milestone_population = 0;
    int next_milestone_population = 0;
    int placement_transition_ms = 0;
    int replacement_population = 0;
    std::array<uint8_t, 25> placement_capabilities{};
};

class BuildCity
{
public:
    explicit BuildCity(const SaveData& save);

    void reset_from_save(const SaveData& save);
    BuildCityUpdateResult update(int delta_ms, const InputFrame& input, SaveData& save);

    [[nodiscard]] BuildCitySnapshot snapshot() const;
    [[nodiscard]] BuildCityConstructionRequest construction_request() const;
    void clear_construction_request();
    void accept_constructed_tower(uint8_t building_type, int population, uint8_t roof);

    [[nodiscard]] const CityTileSave& tile(const SaveData& save, int index) const;
    [[nodiscard]] int placement_capability(const SaveData& save, int index) const;

private:
    BuildCityMode _mode = BuildCityMode::Browse;
    int _total_population = 0;
    int _milestone = 0;
    int _city_level = 0;
    int _max_unlocked_type = 1;
    int _max_trophy_type = 0;
    int _selected_type = 1;
    int _construction_select_ms = 0;
    int _cursor_column = 2;
    int _cursor_row = 2;
    bool _placement_committing = false;
    int _placement_timer_ms = -1;
    uint8_t _pending_type = 0;
    int _pending_population = 0;
    uint8_t _pending_roof = 0;
    int _last_population_delta = 0;
    int _occupied_tiles = 0;
    int _placement_transition_ms = 0;
    int _replacement_population = 0;
    std::array<int, 25> _saved_populations{};
    BuildCityConstructionRequest _request{};
    std::array<uint8_t, 25> _placement_capabilities{};

    void _recalculate_progress(const SaveData& save, bool select_new_unlock);
    void _refresh_capabilities(const SaveData& save);
    [[nodiscard]] bool _placement_valid() const;
    void _start_placement_commit(const SaveData& save);
    BuildCityUpdateResult _finish_placement(SaveData& save);
};
}

#endif
