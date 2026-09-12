#ifndef TB_UI_CONTROLLER_H
#define TB_UI_CONTROLLER_H

#include <cstdint>

#include "tb/app_state.h"
#include "tb/save_data.h"

namespace tb
{
enum class UiScene : uint8_t
{
    Title = 0,
    MainMenu,
    NewGameMenu,
    Settings,
    InstructionsMenu,
    InstructionsPage,
    About,
    TowerGallery,
};

enum class GameRequest : uint8_t
{
    None = 0,
    QuickGame,
    BuildCity,
};

struct UiUpdateResult
{
    bool save_dirty = false;
};

class UiController
{
public:
    explicit UiController(const SaveData& save);

    [[nodiscard]] UiScene scene() const;
    [[nodiscard]] int selection() const;
    [[nodiscard]] int instructions_page() const;
    [[nodiscard]] uint8_t language() const;
    [[nodiscard]] bool sound_enabled() const;
    [[nodiscard]] GameRequest pending_game_request() const;

    UiUpdateResult update(const InputFrame& input, SaveData& save);
    void clear_game_request();

private:
    void _set_scene(UiScene scene);
    void _move_selection(int delta, int count);
    bool _change_language(int delta, SaveData& save);

    UiScene _scene = UiScene::Title;
    int _selection = 0;
    int _instructions_page = 0;
    uint8_t _language = 0;
    bool _sound_enabled = true;
    GameRequest _pending_game_request = GameRequest::None;
};
}

#endif
