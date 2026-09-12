#ifndef TB_GENERATED_TOWER_MESH_ASSETS_H
#define TB_GENERATED_TOWER_MESH_ASSETS_H

#include <cstdint>
#include "bn_sprite_items_tb_mesh_007_p0.h"
#include "bn_sprite_items_tb_mesh_007_p1.h"
#include "bn_sprite_items_tb_mesh_008_p0.h"
#include "bn_sprite_items_tb_mesh_008_p1.h"
#include "bn_sprite_items_tb_mesh_009_p0.h"
#include "bn_sprite_items_tb_mesh_009_p1.h"
#include "bn_sprite_items_tb_mesh_009_p2.h"
#include "bn_sprite_items_tb_mesh_010_p0.h"
#include "bn_sprite_items_tb_mesh_010_p1.h"
#include "bn_sprite_items_tb_mesh_010_p2.h"
#include "bn_sprite_items_tb_mesh_010_p3.h"
#include "bn_sprite_items_tb_mesh_011_p0.h"
#include "bn_sprite_items_tb_mesh_011_p1.h"
#include "bn_sprite_items_tb_mesh_011_p2.h"
#include "bn_sprite_items_tb_mesh_011_p3.h"
#include "bn_sprite_items_tb_mesh_012_p0.h"
#include "bn_sprite_items_tb_mesh_012_p1.h"
#include "bn_sprite_items_tb_mesh_012_p2.h"
#include "bn_sprite_items_tb_mesh_012_p3.h"
#include "bn_sprite_items_tb_mesh_013_p0.h"
#include "bn_sprite_items_tb_mesh_013_p1.h"
#include "bn_sprite_items_tb_mesh_013_p2.h"
#include "bn_sprite_items_tb_mesh_013_p3.h"
#include "bn_sprite_items_tb_mesh_020_p0.h"
#include "bn_sprite_items_tb_mesh_020_p1.h"
#include "bn_sprite_items_tb_mesh_020_p2.h"
#include "bn_sprite_items_tb_mesh_020_p3.h"
#include "bn_sprite_items_tb_mesh_021_p0.h"
#include "bn_sprite_items_tb_mesh_021_p1.h"
#include "bn_sprite_items_tb_mesh_021_p2.h"
#include "bn_sprite_items_tb_mesh_021_p3.h"
#include "bn_sprite_items_tb_mesh_022_p0.h"
#include "bn_sprite_items_tb_mesh_022_p1.h"
#include "bn_sprite_items_tb_mesh_022_p2.h"
#include "bn_sprite_items_tb_mesh_022_p3.h"
#include "bn_sprite_items_tb_mesh_023_p0.h"
#include "bn_sprite_items_tb_mesh_023_p1.h"
#include "bn_sprite_items_tb_mesh_023_p2.h"
#include "bn_sprite_items_tb_mesh_023_p3.h"
#include "bn_sprite_items_tb_mesh_030_p0.h"
#include "bn_sprite_items_tb_mesh_030_p1.h"
#include "bn_sprite_items_tb_mesh_030_p2.h"
#include "bn_sprite_items_tb_mesh_030_p3.h"
#include "bn_sprite_items_tb_mesh_031_p0.h"
#include "bn_sprite_items_tb_mesh_031_p1.h"
#include "bn_sprite_items_tb_mesh_031_p2.h"
#include "bn_sprite_items_tb_mesh_031_p3.h"
#include "bn_sprite_items_tb_mesh_032_p0.h"
#include "bn_sprite_items_tb_mesh_032_p1.h"
#include "bn_sprite_items_tb_mesh_032_p2.h"
#include "bn_sprite_items_tb_mesh_032_p3.h"
#include "bn_sprite_items_tb_mesh_033_p0.h"
#include "bn_sprite_items_tb_mesh_033_p1.h"
#include "bn_sprite_items_tb_mesh_033_p2.h"
#include "bn_sprite_items_tb_mesh_033_p3.h"
#include "bn_sprite_items_tb_mesh_040_p0.h"
#include "bn_sprite_items_tb_mesh_040_p1.h"
#include "bn_sprite_items_tb_mesh_040_p2.h"
#include "bn_sprite_items_tb_mesh_040_p3.h"
#include "bn_sprite_items_tb_mesh_041_p0.h"
#include "bn_sprite_items_tb_mesh_041_p1.h"
#include "bn_sprite_items_tb_mesh_041_p2.h"
#include "bn_sprite_items_tb_mesh_041_p3.h"
#include "bn_sprite_items_tb_mesh_042_p0.h"
#include "bn_sprite_items_tb_mesh_042_p1.h"
#include "bn_sprite_items_tb_mesh_042_p2.h"
#include "bn_sprite_items_tb_mesh_042_p3.h"
#include "bn_sprite_items_tb_mesh_043_p0.h"
#include "bn_sprite_items_tb_mesh_043_p1.h"
#include "bn_sprite_items_tb_mesh_043_p2.h"
#include "bn_sprite_items_tb_mesh_043_p3.h"

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
    { &bn::sprite_items::tb_mesh_007_p0, 13, -27 },
    { &bn::sprite_items::tb_mesh_007_p1, 13, 37 },
};

inline const MeshPartAsset mesh_008_parts[] = {
    { &bn::sprite_items::tb_mesh_008_p0, 11, -34 },
    { &bn::sprite_items::tb_mesh_008_p1, 11, 30 },
};

inline const MeshPartAsset mesh_009_parts[] = {
    { &bn::sprite_items::tb_mesh_009_p0, -28, 20 },
    { &bn::sprite_items::tb_mesh_009_p1, 36, 20 },
    { &bn::sprite_items::tb_mesh_009_p2, 84, 20 },
};

inline const MeshPartAsset mesh_010_parts[] = {
    { &bn::sprite_items::tb_mesh_010_p0, -30, -10 },
    { &bn::sprite_items::tb_mesh_010_p1, 34, -10 },
    { &bn::sprite_items::tb_mesh_010_p2, -30, 54 },
    { &bn::sprite_items::tb_mesh_010_p3, 34, 54 },
};

inline const MeshPartAsset mesh_011_parts[] = {
    { &bn::sprite_items::tb_mesh_011_p0, -30, -10 },
    { &bn::sprite_items::tb_mesh_011_p1, 34, -10 },
    { &bn::sprite_items::tb_mesh_011_p2, -30, 54 },
    { &bn::sprite_items::tb_mesh_011_p3, 34, 54 },
};

inline const MeshPartAsset mesh_012_parts[] = {
    { &bn::sprite_items::tb_mesh_012_p0, -30, -10 },
    { &bn::sprite_items::tb_mesh_012_p1, 34, -10 },
    { &bn::sprite_items::tb_mesh_012_p2, -30, 54 },
    { &bn::sprite_items::tb_mesh_012_p3, 34, 54 },
};

inline const MeshPartAsset mesh_013_parts[] = {
    { &bn::sprite_items::tb_mesh_013_p0, -30, -10 },
    { &bn::sprite_items::tb_mesh_013_p1, 34, -10 },
    { &bn::sprite_items::tb_mesh_013_p2, -30, 54 },
    { &bn::sprite_items::tb_mesh_013_p3, 34, 54 },
};

inline const MeshPartAsset mesh_020_parts[] = {
    { &bn::sprite_items::tb_mesh_020_p0, -30, -10 },
    { &bn::sprite_items::tb_mesh_020_p1, 34, -10 },
    { &bn::sprite_items::tb_mesh_020_p2, -30, 54 },
    { &bn::sprite_items::tb_mesh_020_p3, 34, 54 },
};

inline const MeshPartAsset mesh_021_parts[] = {
    { &bn::sprite_items::tb_mesh_021_p0, -30, -10 },
    { &bn::sprite_items::tb_mesh_021_p1, 34, -10 },
    { &bn::sprite_items::tb_mesh_021_p2, -30, 54 },
    { &bn::sprite_items::tb_mesh_021_p3, 34, 54 },
};

inline const MeshPartAsset mesh_022_parts[] = {
    { &bn::sprite_items::tb_mesh_022_p0, -30, -10 },
    { &bn::sprite_items::tb_mesh_022_p1, 34, -10 },
    { &bn::sprite_items::tb_mesh_022_p2, -30, 54 },
    { &bn::sprite_items::tb_mesh_022_p3, 34, 54 },
};

inline const MeshPartAsset mesh_023_parts[] = {
    { &bn::sprite_items::tb_mesh_023_p0, -30, -10 },
    { &bn::sprite_items::tb_mesh_023_p1, 34, -10 },
    { &bn::sprite_items::tb_mesh_023_p2, -30, 54 },
    { &bn::sprite_items::tb_mesh_023_p3, 34, 54 },
};

inline const MeshPartAsset mesh_030_parts[] = {
    { &bn::sprite_items::tb_mesh_030_p0, -26, -3 },
    { &bn::sprite_items::tb_mesh_030_p1, 38, -3 },
    { &bn::sprite_items::tb_mesh_030_p2, -26, 45 },
    { &bn::sprite_items::tb_mesh_030_p3, 38, 45 },
};

inline const MeshPartAsset mesh_031_parts[] = {
    { &bn::sprite_items::tb_mesh_031_p0, -26, -2 },
    { &bn::sprite_items::tb_mesh_031_p1, 38, -2 },
    { &bn::sprite_items::tb_mesh_031_p2, -26, 46 },
    { &bn::sprite_items::tb_mesh_031_p3, 38, 46 },
};

inline const MeshPartAsset mesh_032_parts[] = {
    { &bn::sprite_items::tb_mesh_032_p0, -22, -1 },
    { &bn::sprite_items::tb_mesh_032_p1, 42, -1 },
    { &bn::sprite_items::tb_mesh_032_p2, -22, 47 },
    { &bn::sprite_items::tb_mesh_032_p3, 42, 47 },
};

inline const MeshPartAsset mesh_033_parts[] = {
    { &bn::sprite_items::tb_mesh_033_p0, -7, -20 },
    { &bn::sprite_items::tb_mesh_033_p1, 41, -20 },
    { &bn::sprite_items::tb_mesh_033_p2, -7, 44 },
    { &bn::sprite_items::tb_mesh_033_p3, 41, 44 },
};

inline const MeshPartAsset mesh_040_parts[] = {
    { &bn::sprite_items::tb_mesh_040_p0, -17, -13 },
    { &bn::sprite_items::tb_mesh_040_p1, 31, -13 },
    { &bn::sprite_items::tb_mesh_040_p2, -17, 51 },
    { &bn::sprite_items::tb_mesh_040_p3, 31, 51 },
};

inline const MeshPartAsset mesh_041_parts[] = {
    { &bn::sprite_items::tb_mesh_041_p0, -11, -16 },
    { &bn::sprite_items::tb_mesh_041_p1, 37, -16 },
    { &bn::sprite_items::tb_mesh_041_p2, -11, 48 },
    { &bn::sprite_items::tb_mesh_041_p3, 37, 48 },
};

inline const MeshPartAsset mesh_042_parts[] = {
    { &bn::sprite_items::tb_mesh_042_p0, -9, -19 },
    { &bn::sprite_items::tb_mesh_042_p1, 39, -19 },
    { &bn::sprite_items::tb_mesh_042_p2, -9, 45 },
    { &bn::sprite_items::tb_mesh_042_p3, 39, 45 },
};

inline const MeshPartAsset mesh_043_parts[] = {
    { &bn::sprite_items::tb_mesh_043_p0, -9, -11 },
    { &bn::sprite_items::tb_mesh_043_p1, 39, -11 },
    { &bn::sprite_items::tb_mesh_043_p2, -9, 53 },
    { &bn::sprite_items::tb_mesh_043_p3, 39, 53 },
};

inline const MeshAsset meshes[] = {
    { 7, mesh_007_parts, 2 },
    { 8, mesh_008_parts, 2 },
    { 9, mesh_009_parts, 3 },
    { 10, mesh_010_parts, 4 },
    { 11, mesh_011_parts, 4 },
    { 12, mesh_012_parts, 4 },
    { 13, mesh_013_parts, 4 },
    { 20, mesh_020_parts, 4 },
    { 21, mesh_021_parts, 4 },
    { 22, mesh_022_parts, 4 },
    { 23, mesh_023_parts, 4 },
    { 30, mesh_030_parts, 4 },
    { 31, mesh_031_parts, 4 },
    { 32, mesh_032_parts, 4 },
    { 33, mesh_033_parts, 4 },
    { 40, mesh_040_parts, 4 },
    { 41, mesh_041_parts, 4 },
    { 42, mesh_042_parts, 4 },
    { 43, mesh_043_parts, 4 },
};
inline constexpr int mesh_count = 19;
}

#endif
