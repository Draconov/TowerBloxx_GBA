#ifndef TB_SCENE_BACKDROP_H
#define TB_SCENE_BACKDROP_H

#include "bn_bg_palettes.h"
#include "bn_color.h"

namespace tb
{
// The captured v1.5.22 menu uses House sky band 1 (#9AC8EA), not the
// device-specific plain-white fallback path. Quantized to GBA 5-bit channels.
inline void set_ui_backdrop()
{
    bn::bg_palettes::set_transparent_color(bn::color(19, 25, 29));
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
