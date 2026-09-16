#ifndef TB_BUILD_CITY_VISUALS_H
#define TB_BUILD_CITY_VISUALS_H

#include <cstdint>

namespace tb
{
[[nodiscard]] uint32_t build_city_valid_lot_rgb(int building_type, int flash_ms);
[[nodiscard]] bool build_city_selector_slot_active(int flash_ms);
[[nodiscard]] bool build_city_preview_raised(int type, int selected_type, int max_unlocked_type);
[[nodiscard]] int build_city_placement_effect_frame(bool replacing, int placement_timer_ms);
[[nodiscard]] int build_city_discard_effect_frame(int placement_timer_ms);
[[nodiscard]] int build_city_changed_population_cells(int old_population, int new_population);

class BuildCityPopulationRoll
{
public:
    void reset();
    void start(int old_population, int new_population);
    void update(int delta_ms);

    [[nodiscard]] bool animating() const;
    [[nodiscard]] bool decreasing() const;
    [[nodiscard]] int changed_cells() const;
    [[nodiscard]] int timer_ms() const;
    [[nodiscard]] int panel_state() const;
    [[nodiscard]] int panel_state_for_cell(int cell_index) const;
    [[nodiscard]] bool use_red_digits() const;

private:
    int _timer_ms = -1;
    int _panel_state = 0;
    int _changed_cells = 0;
    bool _decreasing = false;
};
}

#endif
