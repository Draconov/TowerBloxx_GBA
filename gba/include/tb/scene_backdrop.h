#ifndef TB_SCENE_BACKDROP_H
#define TB_SCENE_BACKDROP_H

#include "bn_bg_palettes.h"
#include "bn_color.h"

namespace tb
{
// Original Nokia menu renderer clears to white before drawing bitmap text.
inline void set_ui_backdrop()
{
    bn::bg_palettes::set_transparent_color(bn::color(31, 31, 31));
}

// House sky starts at #B2D6F2. Quantized to the GBA's 5-bit channels.
inline void set_gameplay_backdrop()
{
    bn::bg_palettes::set_transparent_color(bn::color(22, 26, 30));
}

// Build City also uses a light sky/gradient in the Nokia renderer. The full
// gradient is a later parity layer; never fall back to the black debug field.
inline void set_city_backdrop()
{
    bn::bg_palettes::set_transparent_color(bn::color(22, 26, 30));
}
}

#endif
