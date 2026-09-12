#ifndef TB_UI_SHELL_H
#define TB_UI_SHELL_H

#include "bn_optional.h"
#include "bn_regular_bg_ptr.h"
#include "bn_sprite_ptr.h"
#include "bn_sprite_text_generator.h"
#include "bn_vector.h"

#include "generated/tower_font.h"
#include "generated/tower_ui_assets.h"
#include "tb/app_state.h"
#include "tb/menu_workers.h"
#include "tb/ui_controller.h"

namespace tb
{
class UiShell
{
public:
    UiShell();
    void update(const UiController& controller, const InputFrame& input);
    void hide();

private:
    void _rebuild(const UiController& controller);
    void _show_title(int language);
    void _show_main_menu(const UiController& controller);
    void _show_new_game_menu(const UiController& controller);
    void _show_settings(const UiController& controller);
    void _show_instructions_menu(const UiController& controller);
    void _show_instructions_page(const UiController& controller);
    void _show_about(const UiController& controller);
    void _show_menu(const char* const* labels, int count, int selection);
    void _show_menu_workers();
    void _show_lines(const char* const* lines, int line_count, int page);
    void _show_composite(const generated::UiCompositeAsset& asset, int x, int y, int z_order = 0);
    [[nodiscard]] int _content_page_count(const UiController& controller) const;

    bn::sprite_text_generator _text_generator;
    bn::sprite_text_generator _selected_text_generator;
    bn::optional<bn::regular_bg_ptr> _background;
    bn::vector<bn::sprite_ptr, 128> _sprites;
    UiScene _last_scene = UiScene::TowerGallery;
    int _last_selection = -1;
    int _last_language = -1;
    int _last_sound = -1;
    int _content_page = 0;
    MenuWorkerField _menu_workers;
    int _menu_worker_frame_phase = 0;
    bool _first_update = true;
};
}

#endif
