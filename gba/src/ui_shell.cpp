#include "tb/ui_shell.h"

#include "tb/scene_backdrop.h"

#include "bn_bg_palettes.h"
#include "bn_color.h"

#include "generated/tower_font.h"
#include "generated/tower_localization.h"
#include "generated/tower_ui_assets.h"

namespace tb
{
namespace
{
constexpr int main_menu_indices[] = {17, 18, 19, 21};
constexpr int new_game_indices[] = {91, 92};
constexpr int instructions_indices[] = {91, 92};
constexpr int lines_per_page = generated::instruction_lines_per_page;

int page_count(int lines)
{
    return (lines + lines_per_page - 1) / lines_per_page;
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
    _first_update = true;
}

void UiShell::update(const UiController& controller, const InputFrame& input)
{
    set_ui_backdrop();
    const UiScene scene = controller.scene();
    bool page_changed = false;
    if(scene != _last_scene)
    {
        _content_page = 0;
        page_changed = true;
    }
    else if(scene == UiScene::InstructionsPage || scene == UiScene::About)
    {
        const int pages = _content_page_count(controller);
        if(pages > 1 && input.pressed(Key::Left))
        {
            --_content_page;
            if(_content_page < 0)
            {
                _content_page = pages - 1;
            }
            page_changed = true;
        }
        else if(pages > 1 && input.pressed(Key::Right))
        {
            ++_content_page;
            if(_content_page >= pages)
            {
                _content_page = 0;
            }
            page_changed = true;
        }
    }

    const int language = int(controller.language());
    const int sound = controller.sound_enabled() ? 1 : 0;
    if(_first_update || page_changed || scene != _last_scene || controller.selection() != _last_selection ||
       language != _last_language || sound != _last_sound)
    {
        _rebuild(controller);
        _first_update = false;
        _last_scene = scene;
        _last_selection = controller.selection();
        _last_language = language;
        _last_sound = sound;
    }
}

void UiShell::_rebuild(const UiController& controller)
{
    _sprites.clear();
    switch(controller.scene())
    {
    case UiScene::Title:
        _show_title(controller.language());
        break;
    case UiScene::MainMenu:
        _show_main_menu(controller);
        break;
    case UiScene::NewGameMenu:
        _show_new_game_menu(controller);
        break;
    case UiScene::Settings:
        _show_settings(controller);
        break;
    case UiScene::InstructionsMenu:
        _show_instructions_menu(controller);
        break;
    case UiScene::InstructionsPage:
        _show_instructions_page(controller);
        break;
    case UiScene::About:
        _show_about(controller);
        break;
    case UiScene::TowerGallery:
        break;
    }
}

void UiShell::_show_composite(const generated::UiCompositeAsset& asset, int x, int y, int z_order)
{
    for(int index = 0; index < asset.part_count; ++index)
    {
        const generated::UiSpritePartAsset& part = asset.parts[index];
        bn::sprite_ptr sprite = part.item->create_sprite(x + part.x, y + part.y);
        sprite.set_z_order(z_order);
        _sprites.push_back(sprite);
    }
}

void UiShell::_show_title(int language)
{
    _show_composite(generated::tower_bloxx_logo, 0, -24);
    _text_generator.generate(0, 24, generated::localized_strings[language][0], _sprites);
    _text_generator.generate(-10, 49, "A", _sprites);
    _text_generator.generate(22, 49, generated::localized_strings[language][4], _sprites);
}

void UiShell::_show_menu(const char* const* labels, int count, int selection)
{
    const int top = -((count - 1) * 12) / 2;
    for(int index = 0; index < count; ++index)
    {
        const int y = top + index * 24;
        if(index == selection)
        {
            // Canonical J2ME generic-menu treatment: a light-blue filled row
            // behind white text. Higher z order is drawn first, so the band
            // remains behind the selected glyph sprites.
            _show_composite(generated::menu_highlight, 0, y, 100);
            _selected_text_generator.generate(0, y, labels[index], _sprites);
        }
        else
        {
            _text_generator.generate(0, y, labels[index], _sprites);
        }
    }
}

void UiShell::_show_main_menu(const UiController& controller)
{
    const int language = controller.language();
    const char* labels[4] = {
        generated::localized_strings[language][main_menu_indices[0]],
        generated::localized_strings[language][main_menu_indices[1]],
        generated::localized_strings[language][main_menu_indices[2]],
        generated::localized_strings[language][main_menu_indices[3]],
    };
    _show_menu(labels, 4, controller.selection());
}

void UiShell::_show_new_game_menu(const UiController& controller)
{
    const int language = controller.language();
    const char* labels[2] = {
        generated::localized_strings[language][new_game_indices[0]],
        generated::localized_strings[language][new_game_indices[1]],
    };
    _show_menu(labels, 2, controller.selection());
}

void UiShell::_show_settings(const UiController& controller)
{
    const int language = controller.language();
    const char* labels[2] = {
        generated::localized_strings[language][24],
        generated::localized_strings[language][27],
    };
    _show_menu(labels, 2, controller.selection());
    _text_generator.generate(70, -12,
            generated::localized_strings[language][controller.sound_enabled() ? 13 : 14], _sprites);
    _text_generator.generate(70, 12, generated::locale_names[language], _sprites);
}

void UiShell::_show_instructions_menu(const UiController& controller)
{
    const int language = controller.language();
    const char* labels[2] = {
        generated::localized_strings[language][instructions_indices[0]],
        generated::localized_strings[language][instructions_indices[1]],
    };
    _show_menu(labels, 2, controller.selection());
}

void UiShell::_show_lines(const char* const* lines, int line_count, int page)
{
    const int start = page * lines_per_page;
    int end = start + lines_per_page;
    if(end > line_count)
    {
        end = line_count;
    }
    int y = -62;
    for(int index = start; index < end; ++index)
    {
        _text_generator.generate(0, y, lines[index], _sprites);
        y += 16;
    }
    if(page_count(line_count) > 1)
    {
        _text_generator.generate(0, 66, "<  >", _sprites);
    }
}

void UiShell::_show_instructions_page(const UiController& controller)
{
    const int language = controller.language();
    if(controller.instructions_page() == 0)
    {
        _show_lines(
                generated::quick_game_instruction_lines[language],
                generated::quick_game_instruction_lines_line_counts[language],
                _content_page);
    }
    else
    {
        _show_lines(
                generated::build_city_instruction_lines[language],
                generated::build_city_instruction_lines_line_counts[language],
                _content_page);
    }
}

void UiShell::_show_about(const UiController& controller)
{
    const int language = controller.language();
    _show_composite(generated::sumea_logo, 0, -68);
    _show_lines(generated::about_lines[language], generated::about_lines_line_counts[language], _content_page);
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
