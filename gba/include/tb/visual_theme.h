#ifndef TB_VISUAL_THEME_H
#define TB_VISUAL_THEME_H

#include <cstdint>

#include "tb/save_data.h"

namespace tb
{
enum class VisualTheme : uint8_t
{
    Classic = 0,
    Christmas = 1,
};

inline constexpr uint8_t visual_theme_christmas_mask = 0x01;

[[nodiscard]] inline VisualTheme visual_theme(const SaveData& save)
{
    return (save.reserved[1] & visual_theme_christmas_mask) != 0 ?
            VisualTheme::Christmas : VisualTheme::Classic;
}

inline bool set_visual_theme(SaveData& save, VisualTheme theme)
{
    const uint8_t before = save.reserved[1];
    if(theme == VisualTheme::Christmas)
    {
        save.reserved[1] |= visual_theme_christmas_mask;
    }
    else
    {
        save.reserved[1] &= uint8_t(~visual_theme_christmas_mask);
    }
    return before != save.reserved[1];
}
}

#endif
