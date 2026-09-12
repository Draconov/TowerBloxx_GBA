#ifndef TB_GENERATED_TOWER_UI_ASSETS_H
#define TB_GENERATED_TOWER_UI_ASSETS_H

#include <cstdint>
#include "bn_sprite_items_tower_bloxx_logo_p0.h"
#include "bn_sprite_items_tower_bloxx_logo_p1.h"
#include "bn_sprite_items_sumea_logo_p0.h"

namespace tb::generated
{
struct UiSpritePartAsset
{
    const bn::sprite_item* item;
    int16_t x;
    int16_t y;
};

struct UiCompositeAsset
{
    const UiSpritePartAsset* parts;
    int16_t part_count;
};

inline const UiSpritePartAsset tower_bloxx_logo_parts[] = {
    { &bn::sprite_items::tower_bloxx_logo_p0, -17, 10 },
    { &bn::sprite_items::tower_bloxx_logo_p1, 47, 10 },
};
inline const UiCompositeAsset tower_bloxx_logo = { tower_bloxx_logo_parts, 2 };

inline const UiSpritePartAsset sumea_logo_parts[] = {
    { &bn::sprite_items::sumea_logo_p0, 12, 11 },
};
inline const UiCompositeAsset sumea_logo = { sumea_logo_parts, 1 };

}

#endif
