#include "tb/ui_controller.h"

namespace tb
{
namespace
{
constexpr int locale_count = 5;
constexpr char name_grid[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-.";
constexpr int name_grid_size = int(sizeof(name_grid)) - 1;

bool any_pressed(const InputFrame& input)
{
    return input.pressed_mask != 0;
}

int wrapped(int value, int count)
{
    value %= count;
    if(value < 0)
    {
        value += count;
    }
    return value;
}
}

UiController::UiController(const SaveData& save) :
    _language(save.language < locale_count ? save.language : uint8_t(0)),
    _sound_enabled(save.sound_enabled != 0),
    _name_entry(save.hall_of_fame.last_player_name)
{
}

UiScene UiController::scene() const { return _scene; }
int UiController::selection() const { return _selection; }
int UiController::instructions_page() const { return _instructions_page; }
uint8_t UiController::language() const { return _language; }
bool UiController::sound_enabled() const { return _sound_enabled; }
HallTable UiController::selected_hall_table() const { return _selected_hall_table; }
const HallQualification& UiController::pending_qualification() const { return _pending_qualification; }
uint32_t UiController::pending_score() const { return _pending_score; }
const std::array<char, hall_name_max_length + 1>& UiController::name_entry() const { return _name_entry; }
int UiController::name_cursor() const { return _name_cursor; }

int UiController::root_menu_count() const
{
    return _suspended_session_available ? 6 : 5;
}

RootMenuItem UiController::root_menu_item(int row) const
{
    static constexpr RootMenuItem without_continue[] = {
        RootMenuItem::BuildCity,
        RootMenuItem::QuickGame,
        RootMenuItem::HighScores,
        RootMenuItem::Settings,
        RootMenuItem::Instructions,
    };
    static constexpr RootMenuItem with_continue[] = {
        RootMenuItem::ContinueGame,
        RootMenuItem::BuildCity,
        RootMenuItem::QuickGame,
        RootMenuItem::HighScores,
        RootMenuItem::Settings,
        RootMenuItem::Instructions,
    };
    return _suspended_session_available ? with_continue[row] : without_continue[row];
}

void UiController::set_suspended_session_available(bool value)
{
    if(value == _suspended_session_available)
    {
        return;
    }

    RootMenuItem selected_item = RootMenuItem::BuildCity;
    bool preserve_item = false;
    if(_scene == UiScene::MainMenu && _selection >= 0 && _selection < root_menu_count())
    {
        selected_item = root_menu_item(_selection);
        preserve_item = true;
    }

    _suspended_session_available = value;
    if(_scene == UiScene::MainMenu)
    {
        if(preserve_item && selected_item != RootMenuItem::ContinueGame)
        {
            for(int row = 0; row < root_menu_count(); ++row)
            {
                if(root_menu_item(row) == selected_item)
                {
                    _selection = row;
                    return;
                }
            }
        }
        if(_selection >= root_menu_count())
        {
            _selection = root_menu_count() - 1;
        }
        if(_selection < 0)
        {
            _selection = 0;
        }
    }
}

void UiController::_set_scene(UiScene scene)
{
    _scene = scene;
    _selection = 0;
}

void UiController::_return_to_root(int selection)
{
    _scene = UiScene::MainMenu;
    _selection = selection;
    if(_selection < 0) _selection = 0;
    if(_selection >= root_menu_count()) _selection = root_menu_count() - 1;
}

void UiController::_move_selection(int delta, int count)
{
    _selection = wrapped(_selection + delta, count);
}

bool UiController::_change_language(int delta, SaveData& save)
{
    const uint8_t next = static_cast<uint8_t>(wrapped(int(_language) + delta, locale_count));
    if(next == _language)
    {
        return false;
    }
    _language = next;
    save.language = next;
    return true;
}

ScoreSubmissionBeginResult UiController::begin_score_submission(HallTable table, uint32_t score, SaveData& save, ScoreFlowReturn return_target)
{
    if(table == HallTable::BuildCity && build_city_player_registered(save.hall_of_fame))
    {
        _score_flow_active = false;
        _pending_score = 0;
        _pending_qualification = {};
        return {update_build_city_player_score(save.hall_of_fame, score), false};
    }

    _selected_hall_table = table;
    _pending_score = score;
    _pending_qualification = qualify_hall_score(save.hall_of_fame, table, score);
    _score_flow_return = return_target;
    _score_flow_active = true;
    _selection = 0;
    _scene = _pending_qualification.qualifies ? UiScene::ScoreQualification : UiScene::ScoreFailure;
    return {};
}

UiAction UiController::_finish_score_flow()
{
    const ScoreFlowReturn target = _score_flow_return;
    _score_flow_active = false;
    _pending_score = 0;
    _pending_qualification = {};
    if(target == ScoreFlowReturn::BuildCity)
    {
        _scene = UiScene::MainMenu;
        _selection = 0;
        return UiAction::ReturnToBuildCity;
    }
    _return_to_root(0);
    return UiAction::None;
}

void UiController::_open_name_entry(const SaveData& save)
{
    _name_entry = save.hall_of_fame.last_player_name;
    _name_entry[hall_name_max_length] = '\0';
    _name_cursor = 0;
    _replace_name_on_first_character = true;
    _set_scene(UiScene::NameEntry);
}

int UiController::_name_length() const
{
    int length = 0;
    while(length < hall_name_max_length && _name_entry[length] != '\0')
    {
        ++length;
    }
    return length;
}

void UiController::_append_name_character(char value)
{
    if(_replace_name_on_first_character)
    {
        _name_entry.fill('\0');
        _replace_name_on_first_character = false;
    }
    const int length = _name_length();
    if(length < hall_name_max_length)
    {
        _name_entry[length] = value;
        _name_entry[length + 1] = '\0';
    }
}

void UiController::_delete_name_character()
{
    _replace_name_on_first_character = false;
    const int length = _name_length();
    if(length > 0)
    {
        _name_entry[length - 1] = '\0';
    }
}

void UiController::_confirm_name(SaveData& save, UiUpdateResult& result)
{
    const HallQualification inserted = _selected_hall_table == HallTable::BuildCity ?
        insert_build_city_player_score(save.hall_of_fame, _pending_score, _name_entry.data()) :
        insert_hall_score(save.hall_of_fame, _selected_hall_table, _pending_score, _name_entry.data());
    if(! inserted.qualifies)
    {
        _scene = UiScene::ScoreFailure;
        return;
    }
    normalize_hall_name(_name_entry.data(), save.hall_of_fame.last_player_name);
    _name_entry = save.hall_of_fame.last_player_name;
    result.save_dirty = true;
    _scene = UiScene::HighScoreTable;
    _selection = 0;
}

UiUpdateResult UiController::update(const InputFrame& input, SaveData& save)
{
    UiUpdateResult result;

    switch(_scene)
    {
    case UiScene::PublisherSplash: _set_scene(UiScene::Title); break;

    case UiScene::Title:
        if(any_pressed(input))
        {
            _set_scene(UiScene::MainMenu);
        }
        break;

    case UiScene::MainMenu:
        if(input.pressed(Key::Up))
        {
            _move_selection(-1, root_menu_count());
        }
        else if(input.pressed(Key::Down))
        {
            _move_selection(1, root_menu_count());
        }
        else if(input.pressed(Key::A))
        {
            const RootMenuItem item = root_menu_item(_selection);
            switch(item)
            {
            case RootMenuItem::ContinueGame:
                result.action = UiAction::ResumeSuspended;
                break;
            case RootMenuItem::BuildCity:
            case RootMenuItem::QuickGame:
            {
                const UiAction requested = item == RootMenuItem::BuildCity ? UiAction::StartBuildCity : UiAction::StartQuickGame;
                if(_suspended_session_available)
                {
                    _overwrite_action = requested;
                    _overwrite_root_selection = _selection;
                    _set_scene(UiScene::OverwriteGameConfirm);
                    _selection = 1;
                }
                else
                {
                    result.action = requested;
                }
                break;
            }
            case RootMenuItem::HighScores:
                _set_scene(UiScene::HighScoreSelect);
                break;
            case RootMenuItem::Settings:
                _set_scene(UiScene::Settings);
                break;
            case RootMenuItem::Instructions:
                _set_scene(UiScene::InstructionsMenu);
                break;
            }
        }
        else if(input.pressed(Key::B))
        {
            _set_scene(UiScene::Title);
        }
        break;

    case UiScene::OverwriteGameConfirm:
        if(input.pressed(Key::Up) || input.pressed(Key::Down) || input.pressed(Key::Left) || input.pressed(Key::Right))
        {
            _selection = 1 - _selection;
        }
        else if(input.pressed(Key::A))
        {
            const bool yes = _selection == 0;
            const UiAction action = _overwrite_action;
            const int root_selection = _overwrite_root_selection;
            _overwrite_action = UiAction::None;
            _return_to_root(root_selection);
            if(yes)
            {
                result.action = action;
            }
        }
        else if(input.pressed(Key::B))
        {
            _overwrite_action = UiAction::None;
            _return_to_root(_overwrite_root_selection);
        }
        break;

    case UiScene::Settings:
        if(input.pressed(Key::Up)) _move_selection(-1, 3);
        else if(input.pressed(Key::Down)) _move_selection(1, 3);
        else if(_selection == 0 && (input.pressed(Key::A) || input.pressed(Key::Left) || input.pressed(Key::Right)))
        {
            _sound_enabled = ! _sound_enabled;
            save.sound_enabled = _sound_enabled ? uint8_t(1) : uint8_t(0);
            result.save_dirty = true;
        }
        else if(_selection == 1 && (input.pressed(Key::A) || input.pressed(Key::Right))) result.save_dirty = _change_language(1, save);
        else if(_selection == 1 && input.pressed(Key::Left)) result.save_dirty = _change_language(-1, save);
        else if(_selection == 2 && input.pressed(Key::A)) { _set_scene(UiScene::ResetCityConfirm); _selection = 1; }
        else if(input.pressed(Key::B)) _return_to_root(0);
        break;

    case UiScene::ResetCityConfirm:
        if(input.pressed(Key::Up) || input.pressed(Key::Down) || input.pressed(Key::Left) || input.pressed(Key::Right)) _selection = 1 - _selection;
        else if(input.pressed(Key::A))
        {
            if(_selection == 0) { reset_city_progress(save); result.save_dirty = true; }
            _set_scene(UiScene::Settings); _selection = 2;
        }
        else if(input.pressed(Key::B)) { _set_scene(UiScene::Settings); _selection = 2; }
        break;

    case UiScene::InstructionsMenu:
        if(input.pressed(Key::Up)) _move_selection(-1, 2);
        else if(input.pressed(Key::Down)) _move_selection(1, 2);
        else if(input.pressed(Key::A)) { _instructions_page = _selection; _set_scene(UiScene::InstructionsPage); }
        else if(input.pressed(Key::B)) _return_to_root(0);
        break;

    case UiScene::InstructionsPage:
        if(input.pressed(Key::B)) _set_scene(UiScene::InstructionsMenu);
        break;

    case UiScene::About:
        if(input.pressed(Key::B)) _return_to_root(0);
        break;

    case UiScene::HighScoreSelect:
        if(input.pressed(Key::Up)) _move_selection(-1, 3);
        else if(input.pressed(Key::Down)) _move_selection(1, 3);
        else if(input.pressed(Key::A))
        {
            if(_selection < 2)
            {
                _selected_hall_table = _selection == 0 ? HallTable::BuildCity : HallTable::QuickGame;
                _score_flow_active = false;
                _set_scene(UiScene::HighScoreTable);
            }
            else
            {
                _set_scene(UiScene::ClearHighScoresConfirm);
                _selection = 1;
            }
        }
        else if(input.pressed(Key::B)) _return_to_root(0);
        break;

    case UiScene::HighScoreTable:
        if(input.pressed(Key::B))
        {
            if(_score_flow_active) result.action = _finish_score_flow();
            else _set_scene(UiScene::HighScoreSelect);
        }
        break;

    case UiScene::ClearHighScoresConfirm:
        if(input.pressed(Key::Up) || input.pressed(Key::Down) || input.pressed(Key::Left) || input.pressed(Key::Right)) _selection = 1 - _selection;
        else if(input.pressed(Key::A))
        {
            if(_selection == 0)
            {
                const auto last_name = save.hall_of_fame.last_player_name;
                reset_hall_of_fame(save.hall_of_fame);
                save.hall_of_fame.last_player_name = last_name;
                result.save_dirty = true;
            }
            _set_scene(UiScene::HighScoreSelect);
        }
        else if(input.pressed(Key::B)) _set_scene(UiScene::HighScoreSelect);
        break;

    case UiScene::ScoreQualification:
        if(input.pressed(Key::A) || input.pressed(Key::Start)) _open_name_entry(save);
        else if(input.pressed(Key::B)) result.action = _finish_score_flow();
        break;

    case UiScene::ScoreFailure:
        if(input.pressed(Key::A) || input.pressed(Key::B) || input.pressed(Key::Start)) result.action = _finish_score_flow();
        break;

    case UiScene::NameEntry:
        if(input.pressed(Key::Left)) _name_cursor = wrapped(_name_cursor - 1, name_grid_size);
        else if(input.pressed(Key::Right)) _name_cursor = wrapped(_name_cursor + 1, name_grid_size);
        else if(input.pressed(Key::Up)) _name_cursor = wrapped(_name_cursor - 8, name_grid_size);
        else if(input.pressed(Key::Down)) _name_cursor = wrapped(_name_cursor + 8, name_grid_size);
        else if(input.pressed(Key::A)) _append_name_character(name_grid[_name_cursor]);
        else if(input.pressed(Key::Select)) _append_name_character(' ');
        else if(input.pressed(Key::B))
        {
            if(_name_length() > 0) _delete_name_character();
            else _set_scene(UiScene::ScoreQualification);
        }
        else if(input.pressed(Key::Start)) _confirm_name(save, result);
        break;

    }

    return result;
}
}
