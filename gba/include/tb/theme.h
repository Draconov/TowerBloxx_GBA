#ifndef TB_THEME_H
#define TB_THEME_H

#include <cstdint>

namespace tb
{
enum class GameTheme : uint8_t
{
    Classic = 0,
    Christmas = 1,
};

inline constexpr int game_theme_count = 2;
}

#endif
