#ifndef TB_CHRISTMAS_THEME_ASSETS_H
#define TB_CHRISTMAS_THEME_ASSETS_H

#include "bn_sprite_item.h"
#include "bn_sprite_items_christmas_tower_type_1.h"
#include "bn_sprite_items_christmas_tower_type_2.h"
#include "bn_sprite_items_christmas_tower_type_3.h"
#include "bn_sprite_items_christmas_tower_type_4.h"
#include "bn_sprite_items_christmas_roof_type_1.h"
#include "bn_sprite_items_christmas_roof_type_2.h"
#include "bn_sprite_items_christmas_roof_type_3.h"
#include "bn_sprite_items_christmas_roof_type_4.h"
#include "bn_sprite_items_christmas_construction_tree.h"

namespace tb
{
inline const bn::sprite_item& christmas_tower_item(int building_type)
{
    switch(building_type)
    {
    case 1: return bn::sprite_items::christmas_tower_type_1;
    case 2: return bn::sprite_items::christmas_tower_type_2;
    case 3: return bn::sprite_items::christmas_tower_type_3;
    default: return bn::sprite_items::christmas_tower_type_4;
    }
}

inline const bn::sprite_item& christmas_roof_item(int building_type)
{
    switch(building_type)
    {
    case 1: return bn::sprite_items::christmas_roof_type_1;
    case 2: return bn::sprite_items::christmas_roof_type_2;
    case 3: return bn::sprite_items::christmas_roof_type_3;
    default: return bn::sprite_items::christmas_roof_type_4;
    }
}

inline int christmas_floor_graphics_index(int z_angle_degrees)
{
    // Santa's 225x225 block sheets contain a base block at frame 0 and 23
    // rendered swing poses at frames 1..23. Frame 12 is centred; adjacent
    // frames are five-degree steps, matching the recovered Tower Bloxx swing.
    int rounded_step = z_angle_degrees >= 0 ?
            (z_angle_degrees + 2) / 5 : (z_angle_degrees - 2) / 5;
    if(rounded_step < -11) { rounded_step = -11; }
    if(rounded_step > 11) { rounded_step = 11; }
    return 12 + rounded_step;
}

inline constexpr int christmas_base_graphics_index = 0;
inline constexpr int christmas_normal_roof_graphics_index = 0;
inline constexpr int christmas_trophy_roof_graphics_index = 1;
}

#endif
