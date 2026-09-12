#ifndef TB_GENERATED_TOWER_MESH_ASSETS_H
#define TB_GENERATED_TOWER_MESH_ASSETS_H

#include <cstdint>
#include "bn_sprite_items_tb_mesh_007_p0.h"
#include "bn_sprite_items_tb_mesh_008_p0.h"
#include "bn_sprite_items_tb_mesh_008_p1.h"
#include "bn_sprite_items_tb_mesh_009_p0.h"
#include "bn_sprite_items_tb_mesh_009_p1.h"
#include "bn_sprite_items_tb_mesh_009_p2.h"
#include "bn_sprite_items_tb_mesh_009_p3.h"
#include "bn_sprite_items_tb_mesh_010_p0.h"
#include "bn_sprite_items_tb_mesh_011_p0.h"
#include "bn_sprite_items_tb_mesh_012_p0.h"
#include "bn_sprite_items_tb_mesh_013_p0.h"
#include "bn_sprite_items_tb_mesh_020_p0.h"
#include "bn_sprite_items_tb_mesh_021_p0.h"
#include "bn_sprite_items_tb_mesh_022_p0.h"
#include "bn_sprite_items_tb_mesh_023_p0.h"
#include "bn_sprite_items_tb_mesh_030_p0.h"
#include "bn_sprite_items_tb_mesh_031_p0.h"
#include "bn_sprite_items_tb_mesh_032_p0.h"
#include "bn_sprite_items_tb_mesh_033_p0.h"
#include "bn_sprite_items_tb_mesh_040_p0.h"
#include "bn_sprite_items_tb_mesh_041_p0.h"
#include "bn_sprite_items_tb_mesh_042_p0.h"
#include "bn_sprite_items_tb_mesh_043_p0.h"

namespace tb::generated
{
struct MeshPartAsset
{
    const bn::sprite_item* item;
    int16_t x;
    int16_t y;
};

struct MeshAsset
{
    int16_t mesh_id;
    const MeshPartAsset* parts;
    int16_t part_count;
};

inline const MeshPartAsset mesh_007_parts[] = {
    { &bn::sprite_items::tb_mesh_007_p0, 5, -18 },
};

inline const MeshPartAsset mesh_008_parts[] = {
    { &bn::sprite_items::tb_mesh_008_p0, 10, -48 },
    { &bn::sprite_items::tb_mesh_008_p1, 2, -8 },
};

inline const MeshPartAsset mesh_009_parts[] = {
    { &bn::sprite_items::tb_mesh_009_p0, -88, 16 },
    { &bn::sprite_items::tb_mesh_009_p1, -24, 16 },
    { &bn::sprite_items::tb_mesh_009_p2, 40, 16 },
    { &bn::sprite_items::tb_mesh_009_p3, 104, 16 },
};

inline const MeshPartAsset mesh_010_parts[] = {
    { &bn::sprite_items::tb_mesh_010_p0, 3, 4 },
};

inline const MeshPartAsset mesh_011_parts[] = {
    { &bn::sprite_items::tb_mesh_011_p0, 3, 4 },
};

inline const MeshPartAsset mesh_012_parts[] = {
    { &bn::sprite_items::tb_mesh_012_p0, 3, 4 },
};

inline const MeshPartAsset mesh_013_parts[] = {
    { &bn::sprite_items::tb_mesh_013_p0, 3, 4 },
};

inline const MeshPartAsset mesh_020_parts[] = {
    { &bn::sprite_items::tb_mesh_020_p0, 3, 4 },
};

inline const MeshPartAsset mesh_021_parts[] = {
    { &bn::sprite_items::tb_mesh_021_p0, 3, 4 },
};

inline const MeshPartAsset mesh_022_parts[] = {
    { &bn::sprite_items::tb_mesh_022_p0, 3, 4 },
};

inline const MeshPartAsset mesh_023_parts[] = {
    { &bn::sprite_items::tb_mesh_023_p0, 3, 4 },
};

inline const MeshPartAsset mesh_030_parts[] = {
    { &bn::sprite_items::tb_mesh_030_p0, 0, 2 },
};

inline const MeshPartAsset mesh_031_parts[] = {
    { &bn::sprite_items::tb_mesh_031_p0, 15, 5 },
};

inline const MeshPartAsset mesh_032_parts[] = {
    { &bn::sprite_items::tb_mesh_032_p0, 15, 5 },
};

inline const MeshPartAsset mesh_033_parts[] = {
    { &bn::sprite_items::tb_mesh_033_p0, 0, 10 },
};

inline const MeshPartAsset mesh_040_parts[] = {
    { &bn::sprite_items::tb_mesh_040_p0, 1, 5 },
};

inline const MeshPartAsset mesh_041_parts[] = {
    { &bn::sprite_items::tb_mesh_041_p0, 3, 6 },
};

inline const MeshPartAsset mesh_042_parts[] = {
    { &bn::sprite_items::tb_mesh_042_p0, 2, 1 },
};

inline const MeshPartAsset mesh_043_parts[] = {
    { &bn::sprite_items::tb_mesh_043_p0, 14, 3 },
};

inline const MeshAsset meshes[] = {
    { 7, mesh_007_parts, 1 },
    { 8, mesh_008_parts, 2 },
    { 9, mesh_009_parts, 4 },
    { 10, mesh_010_parts, 1 },
    { 11, mesh_011_parts, 1 },
    { 12, mesh_012_parts, 1 },
    { 13, mesh_013_parts, 1 },
    { 20, mesh_020_parts, 1 },
    { 21, mesh_021_parts, 1 },
    { 22, mesh_022_parts, 1 },
    { 23, mesh_023_parts, 1 },
    { 30, mesh_030_parts, 1 },
    { 31, mesh_031_parts, 1 },
    { 32, mesh_032_parts, 1 },
    { 33, mesh_033_parts, 1 },
    { 40, mesh_040_parts, 1 },
    { 41, mesh_041_parts, 1 },
    { 42, mesh_042_parts, 1 },
    { 43, mesh_043_parts, 1 },
};
inline constexpr int mesh_count = 19;
}

#endif
