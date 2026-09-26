#include "tb/ui_shell.h"

#include "tb/scene_backdrop.h"

#include "bn_bg_palettes.h"
#include "bn_color.h"
#include "bn_sprites.h"
#include "bn_fixed.h"
#include "bn_regular_bg_items_menu_bg.h"
#include "bn_string.h"

#include "generated/tower_font.h"
#include "generated/tower_localization.h"
#include "generated/tower_ui_assets.h"
#include "generated/christmas_assets.h"

namespace tb
{
namespace
{
constexpr int instructions_indices[] = {91, 92};
constexpr int lines_per_page = 5;
constexpr char name_grid[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-.";
constexpr int name_grid_size = int(sizeof(name_grid)) - 1;
constexpr const char* theme_labels[] = {
    "Theme", "Thème", "Tema", "Thema", "Tema",
};
constexpr const char* classic_theme_names[] = {
    "Classic", "Classique", "Classico", "Klassisch", "Clásico",
};
constexpr const char* christmas_theme_names[] = {
    "Christmas", "Noël", "Natale", "Weihnachten", "Navidad",
};

constexpr const char* title_prompt_lines[] = {
    "Press any key to play",
    "Appuyez sur une touche pour jouer",
    "Premi un tasto per giocare",
    "Beliebige Taste drücken zum Starten",
    "Pulsa cualquier tecla para jugar",
};

constexpr const generated::UiCompositeAsset* menu_worker_blue_frames[] = {
    &generated::menu_worker_blue_f0, &generated::menu_worker_blue_f1, &generated::menu_worker_blue_f2,
    &generated::menu_worker_blue_f3, &generated::menu_worker_blue_f4, &generated::menu_worker_blue_f5,
    &generated::menu_worker_blue_f6, &generated::menu_worker_blue_f7, &generated::menu_worker_blue_f8,
    &generated::menu_worker_blue_f9,
};
constexpr const generated::UiCompositeAsset* menu_worker_red_frames[] = {
    &generated::menu_worker_red_f0, &generated::menu_worker_red_f1, &generated::menu_worker_red_f2,
    &generated::menu_worker_red_f3, &generated::menu_worker_red_f4, &generated::menu_worker_red_f5,
    &generated::menu_worker_red_f6, &generated::menu_worker_red_f7, &generated::menu_worker_red_f8,
    &generated::menu_worker_red_f9,
};
constexpr int menu_worker_width = 19;
constexpr int menu_worker_height = 23;
constexpr int christmas_worker_width = 21;
constexpr int christmas_worker_height = 28;
constexpr int menu_cloud_large_width = 105;
constexpr int menu_cloud_large_height = 27;
constexpr int menu_cloud_small_width = 54;
constexpr int menu_cloud_small_height = 14;
constexpr int menu_cloud_z_order = 150;
constexpr int menu_sky_pixels_per_second = 5;
constexpr int dialog_backdrop_z_order = 200;

int page_count(int lines)
{
    return (lines + lines_per_page - 1) / lines_per_page;
}

int root_string_index(RootMenuItem item)
{
    switch(item)
    {
    case RootMenuItem::ContinueGame: return 15;
    case RootMenuItem::BuildCity: return 92;
    case RootMenuItem::QuickGame: return 91;
    case RootMenuItem::HighScores: return 133;
    case RootMenuItem::Settings: return 18;
    case RootMenuItem::Instructions: return 19;
    }
    return 18;
}
}

UiShell::UiShell() :
    _text_generator(generated::tower_font),
    _selected_text_generator(generated::selected_tower_font)
{
    _text_generator.set_center_alignment();
    _selected_text_generator.set_center_alignment();
}

void UiShell::hide()
{
    _sprites.clear();
    _background.reset();
    _menu_workers.reset();
    _menu_clouds.reset();
    _menu_worker_frame_phase = 0;
    _menu_sky_offset = 0;
    _menu_sky_scroll_remainder = 0;
    _title_blink_frame = 0;
    _title_prompt_visible = true;
    _first_update = true;
}

void UiShell::update(const UiController& controller, const SaveData& save, const InputFrame& input)
{
    const UiScene scene = controller.scene();
    if(scene == UiScene::PublisherSplash)
    {
        bn::bg_palettes::set_transparent_color(bn::color(31, 31, 31));
    }
    else
    {
        set_ui_backdrop();
    }
    bool page_changed = false;
    if(scene != _last_scene)
    {
        _content_page = 0;
        page_changed = true;
    }
    else
    {
        const int pages = _content_page_count(controller);
        if((scene == UiScene::InstructionsPage || scene == UiScene::About) && pages > 1 && input.pressed(Key::Up))
        {
            --_content_page;
            if(_content_page < 0) _content_page = pages - 1;
            page_changed = true;
        }
        else if((scene == UiScene::InstructionsPage || scene == UiScene::About) && pages > 1 && input.pressed(Key::Down))
        {
            ++_content_page;
            if(_content_page >= pages) _content_page = 0;
            page_changed = true;
        }
    }

    bool title_blink_changed = false;
    if(scene == UiScene::Title)
    {
        const bool previous_visible = _title_prompt_visible;
        _title_blink_frame = (_title_blink_frame + 1) % 48;
        _title_prompt_visible = _title_blink_frame < 24;
        title_blink_changed = previous_visible != _title_prompt_visible;
    }
    else
    {
        _title_blink_frame = 0;
        _title_prompt_visible = true;
    }

    bool worker_advanced = false;
    bool cloud_advanced = false;
    if(scene == UiScene::MainMenu)
    {
        if(_last_scene != UiScene::MainMenu)
        {
            _menu_workers.reset();
            _menu_clouds.reset();
            _menu_worker_frame_phase = 0;
            _menu_sky_offset = 0;
            _menu_sky_scroll_remainder = 0;
        }

        static constexpr int frame_deltas[] = {16, 17, 17};
        const int delta_ms = frame_deltas[_menu_worker_frame_phase];
        worker_advanced = _menu_workers.update(delta_ms);
        cloud_advanced = _menu_clouds.update(delta_ms);
        _menu_worker_frame_phase = (_menu_worker_frame_phase + 1) % 3;

        _menu_sky_scroll_remainder += delta_ms * menu_sky_pixels_per_second * 256;
        _menu_sky_offset += _menu_sky_scroll_remainder / 1000;
        _menu_sky_scroll_remainder %= 1000;
        _menu_sky_offset %= 256 * 256;
        if(_background)
        {
            _background->set_y(bn::fixed(_menu_sky_offset) / 256);
        }
    }
    else
    {
        _menu_workers.reset();
        _menu_clouds.reset();
        _menu_worker_frame_phase = 0;
        _menu_sky_offset = 0;
        _menu_sky_scroll_remainder = 0;
    }

    const int language = int(controller.language());
    const int sound = controller.sound_enabled() ? 1 : 0;
    const int visual_theme = int(controller.visual_theme());
    const int name_cursor = controller.name_cursor();
    const uint32_t pending_score = controller.pending_score();
    uint64_t name_signature = 1469598103934665603ULL;
    for(char value : controller.name_entry())
    {
        name_signature ^= uint8_t(value);
        name_signature *= 1099511628211ULL;
    }
    const bool name_screen = scene == UiScene::NameEntry;
    if(_first_update || page_changed || title_blink_changed || worker_advanced || cloud_advanced || scene != _last_scene ||
       controller.selection() != _last_selection || language != _last_language || sound != _last_sound ||
       visual_theme != _last_visual_theme ||
       (name_screen && (name_cursor != _last_name_cursor || pending_score != _last_pending_score ||
                        name_signature != _last_name_signature)))
    {
        _rebuild(controller, save);
        _first_update = false;
        _last_scene = scene;
        _last_selection = controller.selection();
        _last_language = language;
        _last_sound = sound;
        _last_visual_theme = visual_theme;
        _last_name_cursor = name_cursor;
        _last_pending_score = pending_score;
        _last_name_signature = name_signature;
    }
}

void UiShell::_rebuild(const UiController& controller, const SaveData& save)
{
    _sprites.clear();
    if(controller.scene() != UiScene::MainMenu)
    {
        _background.reset();
    }

    switch(controller.scene())
    {
    case UiScene::PublisherSplash: _show_publisher_splash(); break;
    case UiScene::Title: _show_title(controller); break;
    case UiScene::MainMenu: _show_root_menu(controller); break;
    case UiScene::OverwriteGameConfirm: _show_overwrite_confirm(controller); break;
    case UiScene::Settings: _show_settings(controller); break;
    case UiScene::ResetCityConfirm: _show_reset_city_confirm(controller); break;
    case UiScene::InstructionsMenu: _show_instructions_menu(controller); break;
    case UiScene::InstructionsPage: _show_instructions_page(controller); break;
    case UiScene::About: _show_about(controller); break;
    case UiScene::HighScoreSelect: _show_high_score_select(controller); break;
    case UiScene::HighScoreTable: _show_high_score_table(controller, save); break;
    case UiScene::ClearHighScoresConfirm: _show_clear_high_scores_confirm(controller); break;
    case UiScene::ScoreQualification: _show_score_message(controller, true); break;
    case UiScene::ScoreFailure: _show_score_message(controller, false); break;
    case UiScene::NameEntry: _show_name_entry(controller); break;
    }
}

void UiShell::_show_composite(const generated::UiCompositeAsset& asset, int x, int y, int z_order)
{
    if(_sprites.max_size() - _sprites.size() < asset.part_count ||
       bn::sprites::available_items_count() < asset.part_count)
    {
        return;
    }
    const int first = _sprites.size();
    for(int index = 0; index < asset.part_count; ++index)
    {
        const generated::UiSpritePartAsset& part = asset.parts[index];
        bn::optional<bn::sprite_ptr> sprite = part.item->create_sprite_optional(x + part.x, y + part.y);
        if(! sprite)
        {
            while(_sprites.size() > first) { _sprites.pop_back(); }
            return;
        }
        sprite->set_z_order(z_order);
        _sprites.push_back(*sprite);
    }
}

void UiShell::_show_publisher_splash()
{
    _show_composite(generated::digital_chocolate_logo, 0, 0);
}

void UiShell::_show_title(const UiController& controller)
{
    const int language = controller.language();
    if(controller.visual_theme() == VisualTheme::Christmas)
    {
        _show_composite(generated::christmas_tower_logo, 0, -24);
    }
    else
    {
        _show_composite(generated::tower_bloxx_logo, 0, -24);
    }
    if(_title_prompt_visible)
    {
        _show_composite(generated::menu_highlight, 0, 49, 100);
        (void) _selected_text_generator.generate_optional(0, 49, title_prompt_lines[language], _sprites);
    }
}

void UiShell::_show_menu(const char* const* labels, int count, int selection)
{
    const int top = -((count - 1) * 12) / 2;
    for(int index = 0; index < count; ++index)
    {
        const int y = top + index * 24;
        if(index == selection)
        {
            _show_composite(generated::menu_highlight, 0, y, 100);
            (void) _selected_text_generator.generate_optional(0, y, labels[index], _sprites);
        }
        else
        {
            (void) _text_generator.generate_optional(0, y, labels[index], _sprites);
        }
    }
}

void UiShell::_show_menu_workers(VisualTheme theme)
{
    for(int index = 0; index < MenuWorkerField::worker_count; ++index)
    {
        const MenuWorker& worker = _menu_workers.worker(index);
        if(worker.y_fixed >= _menu_workers.height_fixed()) continue;
        const int screen_x = (22 * worker.x_fixed) >> 8;
        const int screen_y = (22 * worker.y_fixed) >> 8;
        const int worker_width = theme == VisualTheme::Christmas ? christmas_worker_width : menu_worker_width;
        const int worker_height = theme == VisualTheme::Christmas ? christmas_worker_height : menu_worker_height;
        if(screen_x <= -worker_width || screen_x >= 240 ||
           screen_y <= -worker_height || screen_y >= 160)
        {
            continue;
        }

        const int frame = MenuWorkerField::display_frame(worker.animation_state);
        const generated::UiCompositeAsset& asset = theme == VisualTheme::Christmas ?
                (worker.variant == 1 ? *generated::christmas_worker_blue_frames[frame] :
                                       *generated::christmas_worker_red_frames[frame]) :
                (worker.variant == 1 ? *menu_worker_blue_frames[frame] : *menu_worker_red_frames[frame]);
        const int center_x = screen_x - 120 + worker_width / 2;
        const int center_y = screen_y - 80 + worker_height / 2;
        _show_composite(asset, center_x, center_y, 50);
    }
}

void UiShell::_show_menu_clouds()
{
    for(int index = 0; index < MenuCloudField::cloud_count; ++index)
    {
        const MenuCloud& cloud = _menu_clouds.cloud(index);
        const int screen_x = (22 * cloud.x_fixed) >> 8;
        const int screen_y = (22 * cloud.y_fixed) >> 8;
        const bool large = cloud.type == 1;
        const int width = large ? menu_cloud_large_width : menu_cloud_small_width;
        const int height = large ? menu_cloud_large_height : menu_cloud_small_height;
        if(screen_x <= -width || screen_x >= 240 + width ||
           screen_y <= -height || screen_y >= 160 + height)
        {
            continue;
        }

        const generated::UiCompositeAsset& asset = large ? generated::menu_cloud_large :
                                                           generated::menu_cloud_small;
        _show_composite(asset, screen_x - 120, screen_y - 80, menu_cloud_z_order);
    }
}

void UiShell::_show_root_menu(const UiController& controller)
{
    if(! _background)
    {
        _background = bn::regular_bg_items::menu_bg.create_bg_optional(0, 0);
        if(_background)
        {
            _background->set_priority(3);
            _background->set_y(bn::fixed(_menu_sky_offset) / 256);
        }
    }

    const int language = controller.language();
    const int count = controller.root_menu_count();
    const int top = -30;
    constexpr int spacing = 16;
    if(controller.visual_theme() == VisualTheme::Christmas)
    {
        _show_composite(generated::christmas_tower_logo, 0, -60);
    }
    else
    {
        _show_composite(generated::tower_bloxx_logo, 0, -64);
    }

    for(int row = 0; row < count; ++row)
    {
        const RootMenuItem item = controller.root_menu_item(row);
        const int string_index = root_string_index(item);
        const int y = top + row * spacing;
        if(row == controller.selection())
        {
            _show_composite(generated::menu_highlight, 0, y, 100);
            (void) _selected_text_generator.generate_optional(0, y, generated::localized_strings[language][string_index], _sprites);
        }
        else
        {
            (void) _text_generator.generate_optional(0, y, generated::localized_strings[language][string_index], _sprites);
        }

        const bool christmas = controller.visual_theme() == VisualTheme::Christmas;
        switch(item)
        {
        case RootMenuItem::ContinueGame:
            _show_composite(christmas ? generated::christmas_menu_continue_icon : generated::menu_continue_icon, -74, y);
            break;
        case RootMenuItem::BuildCity:
            _show_composite(christmas ? generated::christmas_menu_build_city_icon : generated::menu_build_city_icon, -74, y);
            break;
        case RootMenuItem::QuickGame:
            _show_composite(christmas ? generated::christmas_menu_quick_game_icon : generated::menu_quick_game_icon, -74, y);
            break;
        case RootMenuItem::HighScores:
            _show_composite(generated::menu_high_scores_icon, -72, y - 1);
            break;
        case RootMenuItem::Instructions:
            _show_composite(generated::menu_instructions_icon, -71, y - 1);
            break;
        case RootMenuItem::Settings:
            _show_composite(christmas ? generated::christmas_menu_settings_icon : generated::menu_settings_icon, -74, y);
            break;
        }
    }

    _show_menu_clouds();
    _show_menu_workers(controller.visual_theme());
    _show_softkeys(language, false, false);
}

void UiShell::_show_overwrite_confirm(const UiController& controller)
{
    _show_dialog_backdrop();
    const int language = controller.language();
    const int line_count = generated::overwrite_game_confirmation_lines_line_counts[language];
    _show_dialog_lines(generated::overwrite_game_confirmation_lines[language], line_count, -28);
    _show_confirmation_options(controller);
    _show_softkeys(language, false, true);
}

void UiShell::_show_settings(const UiController& controller)
{
    const int language = controller.language();
    const char* labels[4] = {
        generated::localized_strings[language][24],
        generated::localized_strings[language][27],
        theme_labels[language],
        generated::localized_strings[language][81],
    };
    _show_menu(labels, 4, controller.selection());
    constexpr int top = -18;
    (void) _text_generator.generate_optional(70, top,
            generated::localized_strings[language][controller.sound_enabled() ? 13 : 14], _sprites);
    (void) _text_generator.generate_optional(70, top + 24, generated::locale_names[language], _sprites);
    const char* theme_name = controller.visual_theme() == VisualTheme::Christmas ?
            christmas_theme_names[language] : classic_theme_names[language];
    (void) _text_generator.generate_optional(70, top + 48, theme_name, _sprites);
    _show_softkeys(language, false, true);
}

void UiShell::_show_dialog_backdrop()
{
    _show_composite(generated::dialog_window, 0, 0, dialog_backdrop_z_order);
}

void UiShell::_show_dialog_lines(const char* const* lines, int line_count, int center_y)
{
    if(line_count <= 0)
    {
        return;
    }

    const int total_height = (line_count - 1) * 16;
    int y = center_y - total_height / 2;
    for(int index = 0; index < line_count; ++index)
    {
        (void) _text_generator.generate_optional(0, y, lines[index], _sprites);
        y += 16;
    }
}

void UiShell::_show_confirmation_options(const UiController& controller)
{
    const int language = controller.language();
    const char* labels[2] = {
        generated::localized_strings[language][9],
        generated::localized_strings[language][10],
    };
    for(int index = 0; index < 2; ++index)
    {
        const int y = 20 + index * 24;
        if(index == controller.selection())
        {
            _show_composite(generated::menu_highlight, 0, y, 100);
            (void) _selected_text_generator.generate_optional(0, y, labels[index], _sprites);
        }
        else
        {
            (void) _text_generator.generate_optional(0, y, labels[index], _sprites);
        }
    }
}

void UiShell::_show_reset_city_confirm(const UiController& controller)
{
    _show_dialog_backdrop();
    const int language = controller.language();
    const int line_count = generated::reset_city_confirmation_lines_line_counts[language];
    _show_dialog_lines(generated::reset_city_confirmation_lines[language], line_count, -28);
    _show_confirmation_options(controller);
    _show_softkeys(language, false, true);
}

void UiShell::_show_high_score_select(const UiController& controller)
{
    const int language = controller.language();
    (void) _text_generator.generate_optional(0, -48, generated::localized_strings[language][126], _sprites);
    const char* labels[3] = {
        generated::localized_strings[language][92],
        generated::localized_strings[language][91],
        generated::localized_strings[language][127],
    };
    _show_menu(labels, 3, controller.selection());
    _show_softkeys(language, false, true);
}

void UiShell::_show_high_score_table(const UiController& controller, const SaveData& save)
{
    const int language = controller.language();
    (void) _text_generator.generate_optional(0, -58, generated::localized_strings[language][133], _sprites);
    const auto& table = save.hall_of_fame.tables[static_cast<int>(controller.selected_hall_table())];
    for(int index = 0; index < hall_entries_per_table; ++index)
    {
        bn::string<48> line;
        line.append(bn::to_string<2>(index + 1));
        line.append(". ");
        line.append(table[index].name.data());
        line.append("  ");
        line.append(bn::to_string<12>(table[index].score));
        (void) _text_generator.generate_optional(0, -20 + index * 24, line, _sprites);
    }
    _show_softkeys(language, false, true);
}

void UiShell::_show_clear_high_scores_confirm(const UiController& controller)
{
    _show_dialog_backdrop();
    const int language = controller.language();
    const char* lines[] = {generated::localized_strings[language][130]};
    _show_dialog_lines(lines, 1, -28);
    _show_confirmation_options(controller);
    _show_softkeys(language, false, true);
}

void UiShell::_show_score_message(const UiController& controller, bool qualified)
{
    const int language = controller.language();
    _show_dialog_backdrop();
    bn::string<96> message;
    const char* source = generated::localized_strings[language][qualified ? 121 : 122];
    const unsigned value = qualified ? controller.pending_qualification().position : 3;
    for(int index = 0; source[index] != '\0'; ++index)
    {
        if(source[index] == '%' && source[index + 1] == 'U')
        {
            message.append(bn::to_string<12>(value));
            ++index;
        }
        else
        {
            char fragment[2] = {source[index], '\0'};
            message.append(fragment);
        }
    }
    (void) _text_generator.generate_optional(0, -18, message, _sprites);
    (void) _text_generator.generate_optional(0, 12, bn::to_string<12>(controller.pending_score()), _sprites);
    _show_softkeys(language, false, true);
}

void UiShell::_show_name_entry(const UiController& controller)
{
    const int language = controller.language();
    constexpr int name_line_y = -48;
    constexpr int name_column_offset = 24;
    (void) _text_generator.generate_optional(0, -68, generated::localized_strings[language][128], _sprites);
    (void) _text_generator.generate_optional(-name_column_offset, name_line_y, generated::localized_strings[language][129], _sprites);
    (void) _text_generator.generate_optional(name_column_offset, name_line_y, controller.name_entry().data(), _sprites);

    constexpr int columns = 8;
    const int selected_row = controller.name_cursor() / columns;
    const int selected_row_y = -18 + selected_row * 17;
    _show_composite(generated::menu_highlight, 0, selected_row_y, 100);

    for(int index = 0; index < name_grid_size; ++index)
    {
        const int row = index / columns;
        const int column = index % columns;
        const int x = -70 + column * 20;
        const int y = -18 + row * 17;
        char label[2] = {name_grid[index], '\0'};
        if(index == controller.name_cursor())
        {
            (void) _selected_text_generator.generate_optional(x, y, label, _sprites);
        }
        else
        {
            (void) _text_generator.generate_optional(x, y, label, _sprites);
        }
    }
    (void) _text_generator.generate_optional(42, 68, "START: OK", _sprites);
    _show_softkeys(language, false, true);
}

void UiShell::_show_softkeys(int language, bool select, bool back)
{
    (void) select;
    if(back)
    {
        (void) _text_generator.generate_optional(92, 70, generated::localized_strings[language][7], _sprites);
    }
}

void UiShell::_show_instructions_menu(const UiController& controller)
{
    const int language = controller.language();
    const char* labels[2] = {
        generated::localized_strings[language][instructions_indices[0]],
        generated::localized_strings[language][instructions_indices[1]],
    };
    _show_menu(labels, 2, controller.selection());
    _show_softkeys(language, false, true);
}

void UiShell::_show_lines(const char* const* lines, int line_count, int page)
{
    const int start = page * lines_per_page;
    int end = start + lines_per_page;
    if(end > line_count) end = line_count;
    const int visible_count = end - start;
    int y = -((visible_count - 1) * 16) / 2;
    for(int index = start; index < end; ++index)
    {
        (void) _text_generator.generate_optional(0, y, lines[index], _sprites);
        y += 16;
    }
    const int pages = page_count(line_count);
    if(page > 0)
    {
        _show_composite(generated::support_nav_f0, 98, -42);
    }
    if(page + 1 < pages)
    {
        _show_composite(generated::support_nav_f1, 98, 42);
    }
}

void UiShell::_show_instructions_page(const UiController& controller)
{
    const int language = controller.language();
    _show_dialog_backdrop();
    if(controller.instructions_page() == 0)
    {
        _show_lines(generated::quick_game_instruction_lines[language],
                    generated::quick_game_instruction_lines_line_counts[language], _content_page);
    }
    else
    {
        _show_lines(generated::build_city_instruction_lines[language],
                    generated::build_city_instruction_lines_line_counts[language], _content_page);
    }
    _show_softkeys(language, false, true);
}

void UiShell::_show_about(const UiController& controller)
{
    const int language = controller.language();
    _show_composite(generated::sumea_logo, 0, -68);
    _show_lines(generated::about_lines[language], generated::about_lines_line_counts[language], _content_page);
    _show_softkeys(language, false, true);
}

int UiShell::_content_page_count(const UiController& controller) const
{
    const int language = controller.language();
    if(controller.scene() == UiScene::About)
    {
        return page_count(generated::about_lines_line_counts[language]);
    }
    if(controller.scene() == UiScene::InstructionsPage)
    {
        if(controller.instructions_page() == 0)
        {
            return page_count(generated::quick_game_instruction_lines_line_counts[language]);
        }
        return page_count(generated::build_city_instruction_lines_line_counts[language]);
    }
    return 1;
}
}
