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
#include "tb/menu_clouds.h"
#include "tb/menu_workers.h"
#include "tb/save_data.h"
#include "tb/ui_controller.h"

namespace tb
{
class UiShell
{
public:
    UiShell();
    void update(const UiController& controller, const SaveData& save, const InputFrame& input);
    void hide();

private:
    void _rebuild(const UiController& controller, const SaveData& save);
    void _show_publisher_splash();
    void _show_title(int language);
    void _show_root_menu(const UiController& controller);
    void _show_overwrite_confirm(const UiController& controller);
    void _show_settings(const UiController& controller);
    void _show_reset_city_confirm(const UiController& controller);
    void _show_instructions_menu(const UiController& controller);
    void _show_instructions_page(const UiController& controller);
    void _show_about(const UiController& controller);
    void _show_high_score_select(const UiController& controller);
    void _show_high_score_table(const UiController& controller, const SaveData& save);
    void _show_clear_high_scores_confirm(const UiController& controller);
    void _show_score_message(const UiController& controller, bool qualified);
    void _show_name_entry(const UiController& controller);
    void _show_softkeys(int language, bool select, bool back);
    void _show_confirmation_options(const UiController& controller);
    void _show_dialog_backdrop();
    void _show_dialog_lines(const char* const* lines, int line_count, int center_y);
    void _show_menu(const char* const* labels, int count, int selection);
    void _show_menu_workers();
    void _show_menu_clouds();
    void _show_lines(const char* const* lines, int line_count, int page);
    void _show_composite(const generated::UiCompositeAsset& asset, int x, int y, int z_order = 0);
    [[nodiscard]] int _content_page_count(const UiController& controller) const;

    bn::sprite_text_generator _text_generator;
    bn::sprite_text_generator _selected_text_generator;
    bn::optional<bn::regular_bg_ptr> _background;
    bn::vector<bn::sprite_ptr, 256> _sprites;
    UiScene _last_scene = UiScene::Title;
    int _last_selection = -1;
    int _last_language = -1;
    int _last_sound = -1;
    int _last_name_cursor = -1;
    uint32_t _last_pending_score = 0;
    uint64_t _last_name_signature = 0;
    int _content_page = 0;
    MenuWorkerField _menu_workers;
    MenuCloudField _menu_clouds;
    int _menu_worker_frame_phase = 0;
    int _menu_sky_offset = 0;
    int _menu_sky_scroll_remainder = 0;
    int _title_blink_frame = 0;
    bool _title_prompt_visible = true;
    bool _first_update = true;
};
}

#endif
