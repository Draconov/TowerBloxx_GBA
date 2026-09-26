#ifndef TB_CONSTRUCTION_BACKDROP_H
#define TB_CONSTRUCTION_BACKDROP_H

#include "bn_optional.h"
#include "bn_regular_bg_ptr.h"
#include "bn_sprite_ptr.h"
#include "bn_vector.h"
#include <cstdint>

#include "tb/visual_theme.h"

namespace tb
{
struct LegacySkyEventSlot
{
    int type = 0;
    int x_eighths = 0;
    int y_eighths = 0;
    int next_spawn_ms = 0;
    int rendered_frame = -1;
    bn::vector<bn::sprite_ptr, 4> sprites;
};

class ConstructionBackdrop
{
public:
    // Preserve unique sky encounters when restoring a suspended scene.
    void start(int camera_y, int clock_ms, bool new_run = true, VisualTheme visual_theme = VisualTheme::Classic);
    void update(int camera_y, int clock_ms);
    void reset();

private:
    void _update_sky(int camera_y);
    void _update_scenery(int camera_y);
    void _update_blinks(int camera_y, int clock_ms);
    void _update_legacy_events(int camera_y, int clock_ms);
    void _spawn_legacy_event(LegacySkyEventSlot& slot, int band, int camera_pixels, int clock_ms);
    void _clear_legacy_event(LegacySkyEventSlot& slot, int clock_ms);
    int _legacy_random(int bound);

    bn::optional<bn::regular_bg_ptr> _sky_background;
    bn::optional<bn::regular_bg_ptr> _scenery_background;
    bn::vector<bn::sprite_ptr, 12> _blink_sprites;
    bn::vector<bn::sprite_ptr, 6> _christmas_scenery_sprites;
    bn::vector<LegacySkyEventSlot, 9> _legacy_events;
    int _legacy_remaining[29] = {};
    uint32_t _spawned_celestial_events = 0;
    uint32_t _legacy_rng = 0x1337B10Cu;
    int _legacy_event_clock_ms = 0;
    int _legacy_event_step_accumulator_ms = 0;
    VisualTheme _visual_theme = VisualTheme::Classic;
    int _sky_index = -1;
    int _scenery_chunk = -1;
};
}

#endif
