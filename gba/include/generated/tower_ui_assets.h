#ifndef TB_GENERATED_TOWER_UI_ASSETS_H
#define TB_GENERATED_TOWER_UI_ASSETS_H

#include <cstdint>
#include "bn_sprite_items_tower_bloxx_logo_p0.h"
#include "bn_sprite_items_tower_bloxx_logo_p1.h"
#include "bn_sprite_items_sumea_logo_p0.h"
#include "bn_sprite_items_menu_highlight_p0.h"
#include "bn_sprite_items_menu_highlight_p1.h"
#include "bn_sprite_items_menu_highlight_p2.h"
#include "bn_sprite_items_menu_highlight_p3.h"
#include "bn_sprite_items_city_building_1_f0_p0.h"
#include "bn_sprite_items_city_building_1_f1_p0.h"
#include "bn_sprite_items_city_building_1_f2_p0.h"
#include "bn_sprite_items_city_building_1_f3_p0.h"
#include "bn_sprite_items_city_building_2_f0_p0.h"
#include "bn_sprite_items_city_building_2_f1_p0.h"
#include "bn_sprite_items_city_building_2_f2_p0.h"
#include "bn_sprite_items_city_building_2_f3_p0.h"
#include "bn_sprite_items_city_building_3_f0_p0.h"
#include "bn_sprite_items_city_building_3_f1_p0.h"
#include "bn_sprite_items_city_building_3_f2_p0.h"
#include "bn_sprite_items_city_building_3_f3_p0.h"
#include "bn_sprite_items_city_building_4_f0_p0.h"
#include "bn_sprite_items_city_building_4_f1_p0.h"
#include "bn_sprite_items_city_building_4_f2_p0.h"
#include "bn_sprite_items_city_building_4_f3_p0.h"
#include "bn_sprite_items_city_lot_f0_p0.h"
#include "bn_sprite_items_city_lot_f1_p0.h"
#include "bn_sprite_items_city_lot_f2_p0.h"
#include "bn_sprite_items_city_lot_f3_p0.h"
#include "bn_sprite_items_city_lot_f4_p0.h"

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

inline const UiSpritePartAsset menu_highlight_parts[] = {
    { &bn::sprite_items::menu_highlight_p0, -83, 8 },
    { &bn::sprite_items::menu_highlight_p1, -19, 8 },
    { &bn::sprite_items::menu_highlight_p2, 45, 8 },
    { &bn::sprite_items::menu_highlight_p3, 109, 8 },
};
inline const UiCompositeAsset menu_highlight = { menu_highlight_parts, 4 };

inline const UiSpritePartAsset city_building_1_f0_parts[] = {
    { &bn::sprite_items::city_building_1_f0_p0, 1, 2 },
};
inline const UiCompositeAsset city_building_1_f0 = { city_building_1_f0_parts, 1 };

inline const UiSpritePartAsset city_building_1_f1_parts[] = {
    { &bn::sprite_items::city_building_1_f1_p0, 1, 1 },
};
inline const UiCompositeAsset city_building_1_f1 = { city_building_1_f1_parts, 1 };

inline const UiSpritePartAsset city_building_1_f2_parts[] = {
    { &bn::sprite_items::city_building_1_f2_p0, 1, 1 },
};
inline const UiCompositeAsset city_building_1_f2 = { city_building_1_f2_parts, 1 };

inline const UiSpritePartAsset city_building_1_f3_parts[] = {
    { &bn::sprite_items::city_building_1_f3_p0, 1, 1 },
};
inline const UiCompositeAsset city_building_1_f3 = { city_building_1_f3_parts, 1 };

inline const UiSpritePartAsset city_building_2_f0_parts[] = {
    { &bn::sprite_items::city_building_2_f0_p0, 0, 2 },
};
inline const UiCompositeAsset city_building_2_f0 = { city_building_2_f0_parts, 1 };

inline const UiSpritePartAsset city_building_2_f1_parts[] = {
    { &bn::sprite_items::city_building_2_f1_p0, 0, 0 },
};
inline const UiCompositeAsset city_building_2_f1 = { city_building_2_f1_parts, 1 };

inline const UiSpritePartAsset city_building_2_f2_parts[] = {
    { &bn::sprite_items::city_building_2_f2_p0, 0, 0 },
};
inline const UiCompositeAsset city_building_2_f2 = { city_building_2_f2_parts, 1 };

inline const UiSpritePartAsset city_building_2_f3_parts[] = {
    { &bn::sprite_items::city_building_2_f3_p0, 0, 0 },
};
inline const UiCompositeAsset city_building_2_f3 = { city_building_2_f3_parts, 1 };

inline const UiSpritePartAsset city_building_3_f0_parts[] = {
    { &bn::sprite_items::city_building_3_f0_p0, 0, 2 },
};
inline const UiCompositeAsset city_building_3_f0 = { city_building_3_f0_parts, 1 };

inline const UiSpritePartAsset city_building_3_f1_parts[] = {
    { &bn::sprite_items::city_building_3_f1_p0, 8, 8 },
};
inline const UiCompositeAsset city_building_3_f1 = { city_building_3_f1_parts, 1 };

inline const UiSpritePartAsset city_building_3_f2_parts[] = {
    { &bn::sprite_items::city_building_3_f2_p0, 8, 8 },
};
inline const UiCompositeAsset city_building_3_f2 = { city_building_3_f2_parts, 1 };

inline const UiSpritePartAsset city_building_3_f3_parts[] = {
    { &bn::sprite_items::city_building_3_f3_p0, 8, 8 },
};
inline const UiCompositeAsset city_building_3_f3 = { city_building_3_f3_parts, 1 };

inline const UiSpritePartAsset city_building_4_f0_parts[] = {
    { &bn::sprite_items::city_building_4_f0_p0, -1, 2 },
};
inline const UiCompositeAsset city_building_4_f0 = { city_building_4_f0_parts, 1 };

inline const UiSpritePartAsset city_building_4_f1_parts[] = {
    { &bn::sprite_items::city_building_4_f1_p0, 7, 7 },
};
inline const UiCompositeAsset city_building_4_f1 = { city_building_4_f1_parts, 1 };

inline const UiSpritePartAsset city_building_4_f2_parts[] = {
    { &bn::sprite_items::city_building_4_f2_p0, 7, 7 },
};
inline const UiCompositeAsset city_building_4_f2 = { city_building_4_f2_parts, 1 };

inline const UiSpritePartAsset city_building_4_f3_parts[] = {
    { &bn::sprite_items::city_building_4_f3_p0, 7, 7 },
};
inline const UiCompositeAsset city_building_4_f3 = { city_building_4_f3_parts, 1 };

inline const UiSpritePartAsset city_lot_f0_parts[] = {
    { &bn::sprite_items::city_lot_f0_p0, 5, 10 },
};
inline const UiCompositeAsset city_lot_f0 = { city_lot_f0_parts, 1 };

inline const UiSpritePartAsset city_lot_f1_parts[] = {
    { &bn::sprite_items::city_lot_f1_p0, 5, 8 },
};
inline const UiCompositeAsset city_lot_f1 = { city_lot_f1_parts, 1 };

inline const UiSpritePartAsset city_lot_f2_parts[] = {
    { &bn::sprite_items::city_lot_f2_p0, 5, 7 },
};
inline const UiCompositeAsset city_lot_f2 = { city_lot_f2_parts, 1 };

inline const UiSpritePartAsset city_lot_f3_parts[] = {
    { &bn::sprite_items::city_lot_f3_p0, 5, 5 },
};
inline const UiCompositeAsset city_lot_f3 = { city_lot_f3_parts, 1 };

inline const UiSpritePartAsset city_lot_f4_parts[] = {
    { &bn::sprite_items::city_lot_f4_p0, -3, 6 },
};
inline const UiCompositeAsset city_lot_f4 = { city_lot_f4_parts, 1 };

}

#endif
