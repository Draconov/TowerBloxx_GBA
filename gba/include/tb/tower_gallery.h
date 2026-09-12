#ifndef TB_TOWER_GALLERY_H
#define TB_TOWER_GALLERY_H

#include "bn_sprite_ptr.h"
#include "bn_vector.h"

#include "tb/app_state.h"

namespace tb
{
class TowerGallery
{
public:
    TowerGallery();
    void update(const InputFrame& input);
    [[nodiscard]] int mesh_index() const;

private:
    void rebuild();

    int _mesh_index = 0;
    bn::vector<bn::sprite_ptr, 4> _sprites;
};
}

#endif
