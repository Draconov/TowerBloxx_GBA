#include "tb/construction_backdrop.h"
#include "tb/sky_event_policy.h"

#include "bn_sprites.h"

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
#include "generated/legacy_high_altitude_assets.h"

namespace tb
{
namespace
{
constexpr int screen_half_width = 120;
constexpr int screen_half_height = 80;
constexpr int legacy_screen_width_eighths = 240 * 8;
constexpr int legacy_screen_height_eighths = 160 * 8;
constexpr int construction_sky_bg_z_order = 1;
constexpr int construction_scenery_bg_z_order = 0;

int sky_color_index(int band)
{
    if(band < 0)
    {
        band = 0;
    }
    return band <= 16 ? band : 9 + ((band - 9) % 8);
}

bn::optional<bn::regular_bg_ptr> create_sky_background(int index)
{
    switch(index)
    {
    case 0: return bn::regular_bg_items::construction_sky_00.create_bg_optional(0, 0);
    case 1: return bn::regular_bg_items::construction_sky_01.create_bg_optional(0, 0);
    case 2: return bn::regular_bg_items::construction_sky_02.create_bg_optional(0, 0);
    case 3: return bn::regular_bg_items::construction_sky_03.create_bg_optional(0, 0);
    case 4: return bn::regular_bg_items::construction_sky_04.create_bg_optional(0, 0);
    case 5: return bn::regular_bg_items::construction_sky_05.create_bg_optional(0, 0);
    case 6: return bn::regular_bg_items::construction_sky_06.create_bg_optional(0, 0);
    case 7: return bn::regular_bg_items::construction_sky_07.create_bg_optional(0, 0);
    case 8: return bn::regular_bg_items::construction_sky_08.create_bg_optional(0, 0);
    case 9: return bn::regular_bg_items::construction_sky_09.create_bg_optional(0, 0);
    case 10: return bn::regular_bg_items::construction_sky_10.create_bg_optional(0, 0);
    case 11: return bn::regular_bg_items::construction_sky_11.create_bg_optional(0, 0);
    case 12: return bn::regular_bg_items::construction_sky_12.create_bg_optional(0, 0);
    case 13: return bn::regular_bg_items::construction_sky_13.create_bg_optional(0, 0);
    case 14: return bn::regular_bg_items::construction_sky_14.create_bg_optional(0, 0);
    case 15: return bn::regular_bg_items::construction_sky_15.create_bg_optional(0, 0);
    default: return bn::regular_bg_items::construction_sky_16.create_bg_optional(0, 0);
    }
}

bn::optional<bn::regular_bg_ptr> create_scenery_background(int index)
{
    switch(index)
    {
    case 0: return bn::regular_bg_items::construction_scenery_0.create_bg_optional(0, 0);
    case 1: return bn::regular_bg_items::construction_scenery_1.create_bg_optional(0, 0);
    default: return bn::regular_bg_items::construction_scenery_2.create_bg_optional(0, 0);
    }
}

void create_legacy_event_sprites(
        const generated::UiCompositeAsset& asset,
        bn::ivector<bn::sprite_ptr>& output)
{
    output.clear();
    // These events are decorative. Keep headroom for the crane, HUD and floors.
    if(output.max_size() < asset.part_count ||
       bn::sprites::available_items_count() < asset.part_count + 12)
    {
        return;
    }
    for(int index = 0; index < asset.part_count; ++index)
    {
        const generated::UiSpritePartAsset& part = asset.parts[index];
        bn::optional<bn::sprite_ptr> sprite = part.item->create_sprite_optional(0, 0);
        if(! sprite)
        {
            output.clear();
            return;
        }
        sprite->set_bg_priority(3);
        sprite->set_z_order(100);
        output.push_back(*sprite);
    }
}

void position_legacy_event_sprites(
        const generated::UiCompositeAsset& asset, int x, int y,
        bn::ivector<bn::sprite_ptr>& sprites)
{
    if(sprites.size() != asset.part_count) { return; }
    for(int index = 0; index < asset.part_count; ++index)
    {
        const generated::UiSpritePartAsset& part = asset.parts[index];
        sprites[index].set_position(x + part.x, y + part.y);
    }
}


}

void ConstructionBackdrop::start(int camera_y, int clock_ms, bool new_run)
{
    reset();
    if(new_run)
    {
        _spawned_celestial_events = 0;
    }
    for(const generated::ConstructionBackgroundDecoration& decoration :
        generated::construction_background_decorations)
    {
        if(decoration.kind == 1)
        {
            if(_blink_sprites.size() >= _blink_sprites.max_size()) { continue; }
            bn::optional<bn::sprite_ptr> blink =
                    bn::sprite_items::construction_high_blink_p0.create_sprite_optional(0, 0);
            if(! blink) { continue; }
            blink->set_bg_priority(3);
            blink->set_z_order(100);
            blink->set_visible(false);
            _blink_sprites.push_back(*blink);
        }
    }

    _legacy_events.clear();
    for(int index = 0; index < 9; ++index)
    {
        _legacy_events.push_back(LegacySkyEventSlot());
    }
    for(int type = 0; type < 29; ++type)
    {
        _legacy_remaining[type] = generated::legacy_event_instance_limits[type];
    }
    _legacy_rng = 0x1337B10Cu;
    _legacy_event_clock_ms = clock_ms;
    _legacy_event_step_accumulator_ms = 0;
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
    _update_legacy_events(camera_y, clock_ms);
}

void ConstructionBackdrop::reset()
{
    _sky_background.reset();
    _scenery_background.reset();
    _blink_sprites.clear();
    _legacy_events.clear();
    _legacy_event_clock_ms = 0;
    _legacy_event_step_accumulator_ms = 0;
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
        _sky_background.reset();
        _sky_index = -1;
        _sky_background = create_sky_background(index);
        if(! _sky_background) { return; } // Wait for BG VRAM reclamation.
        _sky_background->set_priority(3);
        _sky_background->set_z_order(construction_sky_bg_z_order);
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
        _scenery_background.reset();
        _scenery_chunk = -1;
        _scenery_background = create_scenery_background(chunk);
        if(! _scenery_background) { return; } // Retry after core::update().
        _scenery_background->set_priority(3);
        _scenery_background->set_z_order(construction_scenery_bg_z_order);
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

        if(blink_index >= _blink_sprites.size()) { break; }
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



int ConstructionBackdrop::_legacy_random(int bound)
{
    if(bound <= 1)
    {
        return 0;
    }
    _legacy_rng = _legacy_rng * 1664525u + 1013904223u;
    return int((_legacy_rng >> 8) % uint32_t(bound));
}

void ConstructionBackdrop::_clear_legacy_event(LegacySkyEventSlot& slot, int clock_ms)
{
    if(slot.type > 0 && generated::legacy_event_instance_limits[slot.type] >= 0)
    {
        ++_legacy_remaining[slot.type];
    }
    slot.type = 0;
    slot.sprites.clear();
    slot.rendered_frame = -1;
    slot.next_spawn_ms = clock_ms + _legacy_random(2000);
}

void ConstructionBackdrop::_spawn_legacy_event(
        LegacySkyEventSlot& slot, int band, int camera_y, int clock_ms)
{
    int type = 0;

    // Large celestial landmarks and the whale have narrow source altitude
    // bands. Give every unseen unique event priority in its own band so a
    // common cloud/star cannot repeatedly win the random scan and hide it.
    for(int candidate = 1; candidate <= 28; ++candidate)
    {
        const bool in_band = band >= generated::legacy_event_min_band[candidate] &&
                band < generated::legacy_event_max_band[candidate];
        const bool available = generated::legacy_event_instance_limits[candidate] < 0 ||
                _legacy_remaining[candidate] > 0;
        const bool unseen_unique = unique_celestial_event(candidate) &&
                ! (_spawned_celestial_events & celestial_event_flag(candidate));
        if(in_band && available && unseen_unique)
        {
            type = candidate;
            break;
        }
    }

    if(type == 0)
    {
        for(int candidate = 1; candidate <= 28; ++candidate)
        {
            const bool in_band = band >= generated::legacy_event_min_band[candidate] &&
                    band < generated::legacy_event_max_band[candidate];
            const bool available = generated::legacy_event_instance_limits[candidate] < 0 ||
                    _legacy_remaining[candidate] > 0;
            const bool unique_available = ! unique_celestial_event(candidate) ||
                    ! (_spawned_celestial_events & celestial_event_flag(candidate));
            if(in_band && available && unique_available &&
               _legacy_random(100) < generated::legacy_event_spawn_chance[candidate])
            {
                type = candidate;
                break;
            }
        }
    }

    if(type == 0)
    {
        slot.next_spawn_ms = clock_ms + 1000 + _legacy_random(2500);
        return;
    }

    if(generated::legacy_event_instance_limits[type] >= 0)
    {
        --_legacy_remaining[type];
    }

    const int speed = generated::legacy_event_x_speed[type];
    const int extent = generated::legacy_event_extent_eighths[type];
    const int camera_three_quarters = (3 * camera_y) / 4;
    // House.l(int), specialized to the 240x160 GBA viewport. Coordinates stay
    // in the source's 1/8-pixel space until House.e-style projection below.
    if(speed == 0 || _legacy_random(2) == 0)
    {
        // Stationary showcase objects use Java top-left screen coordinates.
        // Keep large planets/whales fully inside the 240px viewport instead
        // of allowing their composite to be clipped on the right.
        const generated::LegacySkyEventAsset& asset = generated::legacy_sky_event_assets[type];
        const int half_width = asset.width / 2;
        constexpr int margin = 4;
        const int min_x = half_width + margin;
        const int max_x = 240 - half_width - margin;
        const int source_x = max_x > min_x ? min_x + _legacy_random(max_x - min_x + 1) : 120;
        slot.x_eighths = source_x * 8;
        slot.y_eighths = camera_three_quarters + extent + _legacy_random(512);
    }
    else
    {
        slot.x_eighths = speed > 0 ? -extent : legacy_screen_width_eighths + extent;
        // Moving clouds, balloons, birds, flyers and planes should enter in
        // the upper part of the viewport. That leaves several block rises for
        // the player to see them instead of spawning near the bottom and
        // disappearing with the next camera movement.
        slot.y_eighths = camera_three_quarters + 96 + _legacy_random(352);
    }
    slot.type = type;
    // A finite *simultaneous* spawn limit alone allows an identical planet to
    // appear again after leaving the screen. Record it for the whole run.
    _spawned_celestial_events |= celestial_event_flag(type);
    slot.rendered_frame = -1;
    slot.next_spawn_ms = 0;
}

void ConstructionBackdrop::_update_legacy_events(int camera_y, int clock_ms)
{
    int delta_ms = clock_ms - _legacy_event_clock_ms;
    if(delta_ms < 0)
    {
        delta_ms = 0;
    }
    _legacy_event_clock_ms = clock_ms;
    _legacy_event_step_accumulator_ms += delta_ms;
    const int movement_steps = _legacy_event_step_accumulator_ms / 25;
    _legacy_event_step_accumulator_ms %= 25;

    const int scaled = (2 * camera_y) / 3;
    const int band = scaled / 2048;
    const int camera_three_quarters = (3 * camera_y) / 4;

    for(LegacySkyEventSlot& slot : _legacy_events)
    {
        if(slot.type == 0)
        {
            if(clock_ms > slot.next_spawn_ms)
            {
                _spawn_legacy_event(slot, band, camera_y, clock_ms);
            }
            continue;
        }

        const int type = slot.type;
        const generated::LegacySkyEventAsset& event_asset = generated::legacy_sky_event_assets[type];
        const int extent = generated::legacy_event_extent_eighths[type];
        slot.x_eighths += generated::legacy_event_x_speed[type] * movement_steps;

        // House.k removes an event only after it has travelled beyond the
        // source horizontal bounds or after the rising camera has pushed it
        // below the bottom edge.  There is deliberately no upper-Y or current
        // band cull: stationary Moon/planet/whale events can wait above the
        // viewport until the camera reaches them.
        const bool outside_source_bounds =
                slot.x_eighths > legacy_screen_width_eighths + extent ||
                slot.x_eighths < -extent ||
                camera_three_quarters - slot.y_eighths > legacy_screen_height_eighths + extent;
        if(outside_source_bounds)
        {
            _clear_legacy_event(slot, clock_ms);
            continue;
        }

        // House.e first projects to normal top-left Java screen coordinates:
        //   (v + 32 * x) >> 8, (w - 32 * (y - 3*camera/4)) >> 8.
        // After specializing v/w to the 240x160 GBA viewport, subtract the
        // screen centre once to convert those coordinates to Butano's origin.
        const int source_screen_x = slot.x_eighths >> 3;
        const int source_screen_y = screen_half_height + ((camera_three_quarters - slot.y_eighths) >> 3);
        const int x = source_screen_x - screen_half_width;
        const int y = source_screen_y - screen_half_height;

        const int frame = event_asset.frame_count > 1 ? (clock_ms / 400) % event_asset.frame_count : 0;
        if(frame != slot.rendered_frame)
        {
            create_legacy_event_sprites(*event_asset.frames[frame], slot.sprites);
            // Retry next frame if the event was omitted due to resource pressure.
            if(slot.sprites.size() != event_asset.frames[frame]->part_count)
            {
                slot.rendered_frame = -1;
                continue;
            }
            slot.rendered_frame = frame;
        }

        // Java Graphics clips off-screen drawing automatically. GBA OBJ
        // coordinates wrap, so explicitly hide composites outside the viewport
        // while keeping their source event slot alive for future camera motion.
        const int half_width = event_asset.width / 2;
        const int half_height = event_asset.height / 2;
        const bool visible = x + half_width >= -screen_half_width &&
                x - half_width < screen_half_width &&
                y + half_height >= -screen_half_height &&
                y - half_height < screen_half_height;
        for(bn::sprite_ptr& sprite : slot.sprites)
        {
            sprite.set_visible(visible);
        }
        if(visible)
        {
            position_legacy_event_sprites(*event_asset.frames[frame], x, y, slot.sprites);
        }
    }
}

}
