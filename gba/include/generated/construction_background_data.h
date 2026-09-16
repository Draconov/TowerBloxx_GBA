#ifndef GENERATED_CONSTRUCTION_BACKGROUND_DATA_H
#define GENERATED_CONSTRUCTION_BACKGROUND_DATA_H

#include <cstdint>

namespace tb::generated
{
struct ConstructionBackgroundDecoration
{
    int x;
    int width;
    int world_y;
    int roof_height;
    uint32_t color;
    int kind;
};

inline constexpr int construction_scenery_chunk_centers[] = {0, 256, 512};
inline constexpr int construction_scenery_max_scroll = 672;
inline constexpr ConstructionBackgroundDecoration construction_background_decorations[] = {
    ConstructionBackgroundDecoration{0, 7, 514, 16, 0x4F98CDu, 0},
    ConstructionBackgroundDecoration{13, 9, 592, 12, 0x5CA0D1u, 1},
    ConstructionBackgroundDecoration{33, 9, 520, 20, 0x5CA0D1u, 1},
    ConstructionBackgroundDecoration{56, 8, 615, 21, 0x4F98CDu, 1},
    ConstructionBackgroundDecoration{79, 8, 462, 16, 0x6FA7D0u, 2},
    ConstructionBackgroundDecoration{100, 7, 574, 14, 0x5CA0D1u, 1},
    ConstructionBackgroundDecoration{117, 5, 573, 14, 0x4F98CDu, 2},
    ConstructionBackgroundDecoration{138, 6, 500, 19, 0x6FA7D0u, 1},
    ConstructionBackgroundDecoration{156, 8, 469, 15, 0x6FA7D0u, 0},
    ConstructionBackgroundDecoration{177, 7, 532, 12, 0x4F98CDu, 2},
    ConstructionBackgroundDecoration{200, 6, 583, 18, 0x6FA7D0u, 1},
    ConstructionBackgroundDecoration{217, 8, 482, 15, 0x6FA7D0u, 2},
};
}

#endif
