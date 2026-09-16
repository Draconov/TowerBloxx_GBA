#include "tb/construction_backdrop.h"

#include "bn_regular_bg_items_construction_scenery_0.h"
#include "bn_regular_bg_items_construction_scenery_1.h"
#include "bn_regular_bg_items_construction_scenery_2.h"
#include "bn_regular_bg_items_construction_sky_00.h"
#include "bn_regular_bg_items_construction_sky_01.h"
#include "bn_regular_bg_items_construction_sky_02.h"
#include "bn_regular_bg_items_construction_sky_03.h"
#include "bn_regular_bg_items_construction_sky_04.h"
#include "bn_regular_bg_items_construction_sky_05.h"
#include "bn_regular_bg_items_construction_sky_06.h"
#include "bn_regular_bg_items_construction_sky_07.h"
#include "bn_regular_bg_items_construction_sky_08.h"
#include "bn_regular_bg_items_construction_sky_09.h"
#include "bn_regular_bg_items_construction_sky_10.h"
#include "bn_regular_bg_items_construction_sky_11.h"
#include "bn_regular_bg_items_construction_sky_12.h"
#include "bn_regular_bg_items_construction_sky_13.h"
#include "bn_regular_bg_items_construction_sky_14.h"
#include "bn_regular_bg_items_construction_sky_15.h"
#include "bn_regular_bg_items_construction_sky_16.h"
#include "bn_sprite_items_construction_high_blink_p0.h"

#include "generated/construction_background_data.h"

namespace tb
{
namespace
{
constexpr int screen_half_width = 120;
constexpr int screen_half_height = 80;

int sky_color_index(int band)
{
    if(band < 0)
    {
        band = 0;
    }
    return band <= 16 ? band : 9 + ((band - 9) % 8);
}

bn::regular_bg_ptr create_sky_background(int index)
{
    switch(index)
    {
    case 0: return bn::regular_bg_items::construction_sky_00.create_bg(0, 0);
    case 1: return bn::regular_bg_items::construction_sky_01.create_bg(0, 0);
    case 2: return bn::regular_bg_items::construction_sky_02.create_bg(0, 0);
    case 3: return bn::regular_bg_items::construction_sky_03.create_bg(0, 0);
    case 4: return bn::regular_bg_items::construction_sky_04.create_bg(0, 0);
    case 5: return bn::regular_bg_items::construction_sky_05.create_bg(0, 0);
    case 6: return bn::regular_bg_items::construction_sky_06.create_bg(0, 0);
    case 7: return bn::regular_bg_items::construction_sky_07.create_bg(0, 0);
    case 8: return bn::regular_bg_items::construction_sky_08.create_bg(0, 0);
    case 9: return bn::regular_bg_items::construction_sky_09.create_bg(0, 0);
    case 10: return bn::regular_bg_items::construction_sky_10.create_bg(0, 0);
    case 11: return bn::regular_bg_items::construction_sky_11.create_bg(0, 0);
    case 12: return bn::regular_bg_items::construction_sky_12.create_bg(0, 0);
    case 13: return bn::regular_bg_items::construction_sky_13.create_bg(0, 0);
    case 14: return bn::regular_bg_items::construction_sky_14.create_bg(0, 0);
    case 15: return bn::regular_bg_items::construction_sky_15.create_bg(0, 0);
    default: return bn::regular_bg_items::construction_sky_16.create_bg(0, 0);
    }
}

bn::regular_bg_ptr create_scenery_background(int index)
{
    switch(index)
    {
    case 0: return bn::regular_bg_items::construction_scenery_0.create_bg(0, 0);
    case 1: return bn::regular_bg_items::construction_scenery_1.create_bg(0, 0);
    default: return bn::regular_bg_items::construction_scenery_2.create_bg(0, 0);
    }
}
}

void ConstructionBackdrop::start(int camera_y, int clock_ms)
{
    reset();
    for(const generated::ConstructionBackgroundDecoration& decoration :
        generated::construction_background_decorations)
    {
        if(decoration.kind == 1)
        {
            bn::sprite_ptr blink = bn::sprite_items::construction_high_blink_p0.create_sprite(0, 0);
            blink.set_bg_priority(2);
            blink.set_z_order(100);
            blink.set_visible(false);
            _blink_sprites.push_back(blink);
        }
    }
    update(camera_y, clock_ms);
}

void ConstructionBackdrop::update(int camera_y, int clock_ms)
{
    if(camera_y < 0)
    {
        camera_y = 0;
    }
    _update_sky(camera_y);
    _update_scenery(camera_y);
    _update_blinks(camera_y, clock_ms);
}

void ConstructionBackdrop::reset()
{
    _sky_background.reset();
    _scenery_background.reset();
    _blink_sprites.clear();
    _sky_index = -1;
    _scenery_chunk = -1;
}

void ConstructionBackdrop::_update_sky(int camera_y)
{
    const int scaled = (2 * camera_y) / 3;
    const int band = scaled / 2048;
    const int horizon = (22 * (scaled % 2048)) >> 8;
    const int index = sky_color_index(band);
    if(! _sky_background || index != _sky_index)
    {
        _sky_background = create_sky_background(index);
        _sky_background->set_priority(3);
        _sky_index = index;
    }
    _sky_background->set_y(horizon - 80);
}

void ConstructionBackdrop::_update_scenery(int camera_y)
{
    const int scroll = ((camera_y - 512) * 22) / 256;
    if(scroll > generated::construction_scenery_max_scroll)
    {
        _scenery_background.reset();
        _scenery_chunk = -1;
        return;
    }

    int normalized_scroll = scroll;
    if(normalized_scroll < 0)
    {
        normalized_scroll = 0;
    }

    int chunk = 0;
    int best_distance = normalized_scroll - generated::construction_scenery_chunk_centers[0];
    if(best_distance < 0)
    {
        best_distance = -best_distance;
    }
    for(int index = 1; index < 3; ++index)
    {
        int distance = normalized_scroll - generated::construction_scenery_chunk_centers[index];
        if(distance < 0)
        {
            distance = -distance;
        }
        if(distance < best_distance)
        {
            best_distance = distance;
            chunk = index;
        }
    }

    if(! _scenery_background || chunk != _scenery_chunk)
    {
        _scenery_background = create_scenery_background(chunk);
        _scenery_background->set_priority(2);
        _scenery_chunk = chunk;
    }

    const int chunk_center = generated::construction_scenery_chunk_centers[chunk];
    _scenery_background->set_y(scroll - chunk_center);
}

void ConstructionBackdrop::_update_blinks(int camera_y, int clock_ms)
{
    int blink_index = 0;
    const bool scenery_visible = _scenery_background.has_value();
    const int camera_pixels = (22 * camera_y) >> 8;
    for(const generated::ConstructionBackgroundDecoration& decoration :
        generated::construction_background_decorations)
    {
        if(decoration.kind != 1)
        {
            continue;
        }

        bn::sprite_ptr& blink = _blink_sprites[blink_index++];
        const int screen_top = screen_half_height - decoration.world_y + camera_pixels - decoration.roof_height;
        const bool phase_visible = ((clock_ms + decoration.world_y) / 200) % 2 == 0;
        const bool onscreen = screen_top >= -4 && screen_top < 160;
        const bool visible = scenery_visible && phase_visible && onscreen;
        blink.set_visible(visible);
        if(visible)
        {
            const int x = decoration.x + decoration.width / 2 + 1 - screen_half_width;
            const int y = screen_top + 1 - screen_half_height;
            blink.set_position(x, y);
        }
    }
}
}
