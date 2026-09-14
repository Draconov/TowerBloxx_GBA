#ifndef TB_BUILD_CITY_VISUALS_H
#define TB_BUILD_CITY_VISUALS_H

#include <cstdint>

namespace tb
{
[[nodiscard]] uint32_t build_city_valid_lot_rgb(int building_type, int flash_ms);
[[nodiscard]] bool build_city_selector_slot_active(int flash_ms);
[[nodiscard]] int build_city_placement_effect_frame(bool replacing, int placement_timer_ms);
}

#endif
