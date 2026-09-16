#ifndef TB_CONSTRUCTION_BACKDROP_H
#define TB_CONSTRUCTION_BACKDROP_H

#include "bn_optional.h"
#include "bn_regular_bg_ptr.h"
#include "bn_sprite_ptr.h"
#include "bn_vector.h"

namespace tb
{
class ConstructionBackdrop
{
public:
    void start(int camera_y, int clock_ms);
    void update(int camera_y, int clock_ms);
    void reset();

private:
    void _update_sky(int camera_y);
    void _update_scenery(int camera_y);
    void _update_blinks(int camera_y, int clock_ms);

    bn::optional<bn::regular_bg_ptr> _sky_background;
    bn::optional<bn::regular_bg_ptr> _scenery_background;
    bn::vector<bn::sprite_ptr, 12> _blink_sprites;
    int _sky_index = -1;
    int _scenery_chunk = -1;
};
}

#endif
