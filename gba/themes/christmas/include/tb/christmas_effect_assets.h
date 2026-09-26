#ifndef TB_CHRISTMAS_EFFECT_ASSETS_H
#define TB_CHRISTMAS_EFFECT_ASSETS_H

#include "generated/tower_ui_assets.h"
#include "bn_sprite_items_christmas_accuracy_star_f0.h"
#include "bn_sprite_items_christmas_accuracy_star_f1.h"
#include "bn_sprite_items_christmas_accuracy_star_f2.h"
#include "bn_sprite_items_christmas_combo_star_f0.h"
#include "bn_sprite_items_christmas_combo_star_f1.h"
#include "bn_sprite_items_christmas_combo_star_f2.h"
#include "bn_sprite_items_christmas_combo_star_f3.h"

namespace tb::christmas
{
inline const generated::UiSpritePartAsset christmas_accuracy_star_f0_parts[] = {
    { &bn::sprite_items::christmas_accuracy_star_f0, 0, 0 },
};
inline const generated::UiCompositeAsset christmas_accuracy_star_f0 = { christmas_accuracy_star_f0_parts, 1 };

inline const generated::UiSpritePartAsset christmas_accuracy_star_f1_parts[] = {
    { &bn::sprite_items::christmas_accuracy_star_f1, 0, 0 },
};
inline const generated::UiCompositeAsset christmas_accuracy_star_f1 = { christmas_accuracy_star_f1_parts, 1 };

inline const generated::UiSpritePartAsset christmas_accuracy_star_f2_parts[] = {
    { &bn::sprite_items::christmas_accuracy_star_f2, 0, 0 },
};
inline const generated::UiCompositeAsset christmas_accuracy_star_f2 = { christmas_accuracy_star_f2_parts, 1 };

inline const generated::UiSpritePartAsset christmas_combo_star_f0_parts[] = {
    { &bn::sprite_items::christmas_combo_star_f0, 0, 0 },
};
inline const generated::UiCompositeAsset christmas_combo_star_f0 = { christmas_combo_star_f0_parts, 1 };

inline const generated::UiSpritePartAsset christmas_combo_star_f1_parts[] = {
    { &bn::sprite_items::christmas_combo_star_f1, 0, 0 },
};
inline const generated::UiCompositeAsset christmas_combo_star_f1 = { christmas_combo_star_f1_parts, 1 };

inline const generated::UiSpritePartAsset christmas_combo_star_f2_parts[] = {
    { &bn::sprite_items::christmas_combo_star_f2, 0, 0 },
};
inline const generated::UiCompositeAsset christmas_combo_star_f2 = { christmas_combo_star_f2_parts, 1 };

inline const generated::UiSpritePartAsset christmas_combo_star_f3_parts[] = {
    { &bn::sprite_items::christmas_combo_star_f3, 0, 0 },
};
inline const generated::UiCompositeAsset christmas_combo_star_f3 = { christmas_combo_star_f3_parts, 1 };

inline const generated::UiCompositeAsset* const accuracy_star_frames[3] = {
    &christmas_accuracy_star_f0, &christmas_accuracy_star_f1, &christmas_accuracy_star_f2
};

inline const generated::UiCompositeAsset* const combo_star_frames[4] = {
    &christmas_combo_star_f0, &christmas_combo_star_f1, &christmas_combo_star_f2, &christmas_combo_star_f3
};

}

#endif
