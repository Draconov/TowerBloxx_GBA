#include "tb/ui_controller.h"

namespace tb
{
namespace
{
constexpr int locale_count = 5;
}

UiController::UiController(const SaveData& save) :
    _language(save.language < locale_count ? save.language : uint8_t(0)),
    _sound_enabled(save.sound_enabled != 0)
{
}

UiScene UiController::scene() const
{
    return _scene;
}

int UiController::selection() const
{
    return _selection;
}

int UiController::instructions_page() const
{
    return _instructions_page;
}

uint8_t UiController::language() const
{
    return _language;
}

bool UiController::sound_enabled() const
{
    return _sound_enabled;
}

GameRequest UiController::pending_game_request() const
{
    return _pending_game_request;
}

void UiController::clear_game_request()
{
    _pending_game_request = GameRequest::None;
}

void UiController::_set_scene(UiScene scene)
{
    _scene = scene;
    _selection = 0;
}

void UiController::_move_selection(int delta, int count)
{
    _selection = (_selection + delta) % count;
    if(_selection < 0)
    {
        _selection += count;
    }
}

bool UiController::_change_language(int delta, SaveData& save)
{
    int value = (int(_language) + delta) % locale_count;
    if(value < 0)
    {
        value += locale_count;
    }
    const uint8_t next = static_cast<uint8_t>(value);
    if(next == _language)
    {
        return false;
    }
    _language = next;
    save.language = next;
    return true;
}

UiUpdateResult UiController::update(const InputFrame& input, SaveData& save)
{
    UiUpdateResult result;

    switch(_scene)
    {
    case UiScene::Title:
        if(input.pressed(Key::A) || input.pressed(Key::Start))
        {
            _set_scene(UiScene::MainMenu);
        }
        break;

    case UiScene::MainMenu:
        if(input.pressed(Key::Up))
        {
            _move_selection(-1, 4);
        }
        else if(input.pressed(Key::Down))
        {
            _move_selection(1, 4);
        }
        else if(input.pressed(Key::A))
        {
            switch(_selection)
            {
            case 0: _set_scene(UiScene::NewGameMenu); break;
            case 1: _set_scene(UiScene::Settings); break;
            case 2: _set_scene(UiScene::InstructionsMenu); break;
            default: _set_scene(UiScene::About); break;
            }
        }
        else if(input.pressed(Key::B))
        {
            _set_scene(UiScene::Title);
        }
        break;

    case UiScene::NewGameMenu:
        if(input.pressed(Key::Up))
        {
            _move_selection(-1, 2);
        }
        else if(input.pressed(Key::Down))
        {
            _move_selection(1, 2);
        }
        else if(input.pressed(Key::A))
        {
            _pending_game_request = _selection == 0 ? GameRequest::QuickGame : GameRequest::BuildCity;
        }
        else if(input.pressed(Key::B))
        {
            _set_scene(UiScene::MainMenu);
        }
        break;

    case UiScene::Settings:
        if(input.pressed(Key::Up))
        {
            _move_selection(-1, 2);
        }
        else if(input.pressed(Key::Down))
        {
            _move_selection(1, 2);
        }
        else if(_selection == 0 && (input.pressed(Key::A) || input.pressed(Key::Left) || input.pressed(Key::Right)))
        {
            _sound_enabled = ! _sound_enabled;
            save.sound_enabled = _sound_enabled ? uint8_t(1) : uint8_t(0);
            result.save_dirty = true;
        }
        else if(_selection == 1 && (input.pressed(Key::A) || input.pressed(Key::Right)))
        {
            result.save_dirty = _change_language(1, save);
        }
        else if(_selection == 1 && input.pressed(Key::Left))
        {
            result.save_dirty = _change_language(-1, save);
        }
        else if(input.pressed(Key::B))
        {
            _set_scene(UiScene::MainMenu);
        }
        break;

    case UiScene::InstructionsMenu:
        if(input.pressed(Key::Up))
        {
            _move_selection(-1, 2);
        }
        else if(input.pressed(Key::Down))
        {
            _move_selection(1, 2);
        }
        else if(input.pressed(Key::A))
        {
            _instructions_page = _selection;
            _set_scene(UiScene::InstructionsPage);
        }
        else if(input.pressed(Key::B))
        {
            _set_scene(UiScene::MainMenu);
        }
        break;

    case UiScene::InstructionsPage:
        if(input.pressed(Key::B))
        {
            _set_scene(UiScene::InstructionsMenu);
        }
        break;

    case UiScene::About:
        if(input.pressed(Key::B))
        {
            _set_scene(UiScene::MainMenu);
        }
        break;

    case UiScene::TowerGallery:
        if(input.pressed(Key::B))
        {
            _set_scene(UiScene::MainMenu);
        }
        break;
    }

    return result;
}
}
