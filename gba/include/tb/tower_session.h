#ifndef TB_TOWER_SESSION_H
#define TB_TOWER_SESSION_H

#include <cstdint>

namespace tb
{
enum class RuntimeScene : uint8_t
{
    Ui = 0,
    QuickGame,
    BuildCity,
    Construction,
};

enum class SuspendedSessionKind : uint8_t
{
    None = 0,
    QuickGame,
    BuildCityConstruction,
};

class TowerSessionCoordinator
{
public:
    [[nodiscard]] RuntimeScene foreground() const;
    [[nodiscard]] bool has_suspended() const;
    [[nodiscard]] SuspendedSessionKind suspended_kind() const;

    void show_ui();
    void start_quick_game();
    void start_build_city();
    void start_construction();
    void suspend_quick_game();
    void suspend_construction();
    SuspendedSessionKind resume_suspended();
    void return_to_build_city();
    void discard_suspended();

private:
    RuntimeScene _foreground = RuntimeScene::Ui;
    SuspendedSessionKind _suspended = SuspendedSessionKind::None;
};
}

#endif
