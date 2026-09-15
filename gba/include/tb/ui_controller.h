#ifndef TB_UI_CONTROLLER_H
#define TB_UI_CONTROLLER_H

#include <array>
#include <cstdint>

#include "tb/app_state.h"
#include "tb/hall_of_fame.h"
#include "tb/save_data.h"

namespace tb
{
enum class UiScene : uint8_t
{
    PublisherSplash = 0,
    Title,
    MainMenu,
    OverwriteGameConfirm,
    Settings,
    ResetCityConfirm,
    InstructionsMenu,
    InstructionsPage,
    About,
    HighScoreSelect,
    HighScoreTable,
    ClearHighScoresConfirm,
    ScoreQualification,
    ScoreFailure,
    NameEntry,
    TowerGallery,
};

enum class RootMenuItem : uint8_t
{
    ContinueGame = 0,
    BuildCity,
    QuickGame,
    HighScores,
    Settings,
    Instructions,
};

enum class UiAction : uint8_t
{
    None = 0,
    StartQuickGame,
    StartBuildCity,
    ResumeSuspended,
    ReturnToBuildCity,
};

enum class ScoreFlowReturn : uint8_t
{
    RootMenu = 0,
    BuildCity,
};

struct UiUpdateResult
{
    bool save_dirty = false;
    UiAction action = UiAction::None;
};

struct ScoreSubmissionBeginResult
{
    bool save_dirty = false;
    bool requires_ui = true;
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

    [[nodiscard]] int root_menu_count() const;
    [[nodiscard]] RootMenuItem root_menu_item(int row) const;
    void set_suspended_session_available(bool value);

    [[nodiscard]] HallTable selected_hall_table() const;
    [[nodiscard]] const HallQualification& pending_qualification() const;
    [[nodiscard]] uint32_t pending_score() const;
    [[nodiscard]] const std::array<char, hall_name_max_length + 1>& name_entry() const;
    [[nodiscard]] int name_cursor() const;
    ScoreSubmissionBeginResult begin_score_submission(HallTable table, uint32_t score, SaveData& save, ScoreFlowReturn return_target);

    UiUpdateResult update(const InputFrame& input, SaveData& save);


private:
    void _set_scene(UiScene scene);
    void _return_to_root(int selection = 0);
    void _move_selection(int delta, int count);
    bool _change_language(int delta, SaveData& save);
    UiAction _finish_score_flow();
    void _open_name_entry(const SaveData& save);
    void _append_name_character(char value);
    void _delete_name_character();
    void _confirm_name(SaveData& save, UiUpdateResult& result);
    [[nodiscard]] int _name_length() const;

    UiScene _scene = UiScene::Title;
    int _selection = 0;
    int _instructions_page = 0;
    uint8_t _language = 0;
    bool _sound_enabled = true;
    bool _suspended_session_available = false;
    int _publisher_frame_phase = 0;

    HallTable _selected_hall_table = HallTable::BuildCity;
    HallQualification _pending_qualification{};
    uint32_t _pending_score = 0;
    ScoreFlowReturn _score_flow_return = ScoreFlowReturn::RootMenu;
    bool _score_flow_active = false;
    std::array<char, hall_name_max_length + 1> _name_entry{};
    int _name_cursor = 0;
    bool _replace_name_on_first_character = false;

    UiAction _overwrite_action = UiAction::None;
    int _overwrite_root_selection = 0;

};
}

#endif
