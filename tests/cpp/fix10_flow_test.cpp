#include <cassert>
#include <cstring>
#include <iostream>

#include "tb/app_state.h"
#include "tb/hall_of_fame.h"
#include "tb/save_data.h"
#include "tb/tower_session.h"
#include "tb/ui_controller.h"

namespace
{
tb::InputFrame fresh(tb::Key key)
{
    return tb::InputFrame{tb::key_mask(key), tb::key_mask(key)};
}
}

int main()
{
    tb::SaveData save = tb::make_default_save();
    tb::UiController ui(save);
    tb::TowerSessionCoordinator session;

    ui.update(fresh(tb::Key::A), save);
    assert(ui.scene() == tb::UiScene::MainMenu);
    assert(ui.root_menu_count() == 5);

    ui.update(fresh(tb::Key::Down), save); // Quick Game.
    auto result = ui.update(fresh(tb::Key::A), save);
    assert(result.action == tb::UiAction::StartQuickGame);
    session.start_quick_game();
    assert(session.foreground() == tb::RuntimeScene::QuickGame);

    session.suspend_quick_game();
    ui.set_suspended_session_available(session.has_suspended());
    assert(session.suspended_kind() == tb::SuspendedSessionKind::QuickGame);
    assert(ui.root_menu_count() == 6);

    // Visiting Settings does not touch the suspended session.
    while(ui.root_menu_item(ui.selection()) != tb::RootMenuItem::Settings)
    {
        ui.update(fresh(tb::Key::Down), save);
    }
    ui.update(fresh(tb::Key::A), save);
    assert(ui.scene() == tb::UiScene::Settings);
    assert(session.suspended_kind() == tb::SuspendedSessionKind::QuickGame);
    ui.update(fresh(tb::Key::B), save);

    while(ui.root_menu_item(ui.selection()) != tb::RootMenuItem::BuildCity)
    {
        ui.update(fresh(tb::Key::Up), save);
    }
    ui.update(fresh(tb::Key::A), save);
    assert(ui.scene() == tb::UiScene::OverwriteGameConfirm);
    assert(ui.selection() == 1); // No.
    ui.update(fresh(tb::Key::A), save);
    assert(ui.scene() == tb::UiScene::MainMenu);
    assert(session.suspended_kind() == tb::SuspendedSessionKind::QuickGame);

    ui.update(fresh(tb::Key::A), save);
    ui.update(fresh(tb::Key::Up), save); // Yes.
    result = ui.update(fresh(tb::Key::A), save);
    assert(result.action == tb::UiAction::StartBuildCity);
    session.discard_suspended();
    session.start_build_city();
    assert(! session.has_suspended());
    assert(session.foreground() == tb::RuntimeScene::BuildCity);

    // Qualifying Quick Game score only changes its table.
    ui.begin_score_submission(tb::HallTable::QuickGame, 1000, save, tb::ScoreFlowReturn::RootMenu);
    assert(ui.scene() == tb::UiScene::ScoreQualification);
    ui.update(fresh(tb::Key::A), save);
    assert(ui.scene() == tb::UiScene::NameEntry);
    ui.update(fresh(tb::Key::A), save); // Replace SUMEA with A.
    result = ui.update(fresh(tb::Key::Start), save);
    assert(result.save_dirty);
    assert(ui.scene() == tb::UiScene::HighScoreTable);
    assert(save.hall_of_fame.tables[1][0].score == 1000);
    assert(std::strcmp(save.hall_of_fame.tables[1][0].name.data(), "A") == 0);
    assert(save.hall_of_fame.tables[0][0].score == 0);
    result = ui.update(fresh(tb::Key::B), save);
    assert(result.action == tb::UiAction::None);
    assert(ui.scene() == tb::UiScene::MainMenu);

    ui.begin_score_submission(tb::HallTable::BuildCity, 2000, save, tb::ScoreFlowReturn::BuildCity);
    ui.update(fresh(tb::Key::A), save);
    ui.update(fresh(tb::Key::A), save);
    result = ui.update(fresh(tb::Key::Start), save);
    assert(result.save_dirty);
    assert(save.hall_of_fame.tables[0][0].score == 2000);
    result = ui.update(fresh(tb::Key::B), save);
    assert(result.action == tb::UiAction::ReturnToBuildCity);

    std::cout << "fix10 flow ok\n";
}
