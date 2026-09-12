#include "tb/tower_gallery.h"

#include "generated/tower_mesh_assets.h"

namespace tb
{
TowerGallery::TowerGallery()
{
    rebuild();
}

void TowerGallery::update(const InputFrame& input)
{
    if(input.pressed(Key::Left))
    {
        --_mesh_index;
        if(_mesh_index < 0)
        {
            _mesh_index = generated::mesh_count - 1;
        }
        rebuild();
    }
    else if(input.pressed(Key::Right))
    {
        ++_mesh_index;
        if(_mesh_index >= generated::mesh_count)
        {
            _mesh_index = 0;
        }
        rebuild();
    }
}

int TowerGallery::mesh_index() const
{
    return _mesh_index;
}

void TowerGallery::rebuild()
{
    _sprites.clear();
    const generated::MeshAsset& mesh = generated::meshes[_mesh_index];
    for(int index = 0; index < mesh.part_count; ++index)
    {
        const generated::MeshPartAsset& part = mesh.parts[index];
        _sprites.push_back(part.item->create_sprite(part.x, part.y));
    }
}
}
