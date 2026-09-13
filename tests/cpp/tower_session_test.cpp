#include <cassert>
#include <iostream>
#include "tb/tower_session.h"

int main()
{
    tb::TowerSessionCoordinator session;
    assert(session.foreground() == tb::RuntimeScene::Ui);
    assert(! session.has_suspended());

    session.start_quick_game();
    assert(session.foreground() == tb::RuntimeScene::QuickGame);
    session.suspend_quick_game();
    assert(session.foreground() == tb::RuntimeScene::Ui);
    assert(session.suspended_kind() == tb::SuspendedSessionKind::QuickGame);
    assert(session.resume_suspended() == tb::SuspendedSessionKind::QuickGame);
    assert(session.foreground() == tb::RuntimeScene::QuickGame);

    session.start_build_city();
    session.start_construction();
    session.suspend_construction();
    assert(session.suspended_kind() == tb::SuspendedSessionKind::BuildCityConstruction);
    session.discard_suspended();
    assert(! session.has_suspended());
    assert(session.foreground() == tb::RuntimeScene::Ui);

    session.return_to_build_city();
    assert(session.foreground() == tb::RuntimeScene::BuildCity);
    std::cout << "tower session ok\n";
}
