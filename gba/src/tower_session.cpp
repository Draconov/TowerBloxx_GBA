#include "tb/tower_session.h"

namespace tb
{
RuntimeScene TowerSessionCoordinator::foreground() const { return _foreground; }
bool TowerSessionCoordinator::has_suspended() const { return _suspended != SuspendedSessionKind::None; }
SuspendedSessionKind TowerSessionCoordinator::suspended_kind() const { return _suspended; }

void TowerSessionCoordinator::show_ui()
{
    _foreground = RuntimeScene::Ui;
}

void TowerSessionCoordinator::start_quick_game()
{
    _foreground = RuntimeScene::QuickGame;
}

void TowerSessionCoordinator::start_build_city()
{
    _foreground = RuntimeScene::BuildCity;
}

void TowerSessionCoordinator::start_construction()
{
    _foreground = RuntimeScene::Construction;
}

void TowerSessionCoordinator::suspend_quick_game()
{
    _suspended = SuspendedSessionKind::QuickGame;
    _foreground = RuntimeScene::Ui;
}

void TowerSessionCoordinator::suspend_construction()
{
    _suspended = SuspendedSessionKind::BuildCityConstruction;
    _foreground = RuntimeScene::Ui;
}

SuspendedSessionKind TowerSessionCoordinator::resume_suspended()
{
    const SuspendedSessionKind previous = _suspended;
    _suspended = SuspendedSessionKind::None;
    if(previous == SuspendedSessionKind::QuickGame)
    {
        _foreground = RuntimeScene::QuickGame;
    }
    else if(previous == SuspendedSessionKind::BuildCityConstruction)
    {
        _foreground = RuntimeScene::Construction;
    }
    return previous;
}

void TowerSessionCoordinator::return_to_build_city()
{
    _foreground = RuntimeScene::BuildCity;
}

void TowerSessionCoordinator::discard_suspended()
{
    _suspended = SuspendedSessionKind::None;
    _foreground = RuntimeScene::Ui;
}
}
