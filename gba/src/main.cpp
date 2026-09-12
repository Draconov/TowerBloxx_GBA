#include "bn_core.h"
#include "bn_keypad.h"

#include "tb/app_state.h"
#include "tb/quick_game_scene.h"
#include "tb/save_store.h"
#include "tb/ui_controller.h"
#include "tb/ui_shell.h"

namespace
{
uint16_t held_keys()
{
    uint16_t result = 0;
    if(bn::keypad::a_held()) { result |= tb::key_mask(tb::Key::A); }
    if(bn::keypad::b_held()) { result |= tb::key_mask(tb::Key::B); }
    if(bn::keypad::select_held()) { result |= tb::key_mask(tb::Key::Select); }
    if(bn::keypad::start_held()) { result |= tb::key_mask(tb::Key::Start); }
    if(bn::keypad::right_held()) { result |= tb::key_mask(tb::Key::Right); }
    if(bn::keypad::left_held()) { result |= tb::key_mask(tb::Key::Left); }
    if(bn::keypad::up_held()) { result |= tb::key_mask(tb::Key::Up); }
    if(bn::keypad::down_held()) { result |= tb::key_mask(tb::Key::Down); }
    return result;
}
}

int main()
{
    bn::core::init();

    tb::AppState app;
    tb::SaveData save = tb::load_save();
    tb::UiController controller(save);
    tb::UiShell ui;
    tb::QuickGameScene quick_game;

    while(true)
    {
        const tb::InputFrame input = app.update_input(held_keys());

        if(quick_game.active())
        {
            const tb::QuickGameSceneUpdateResult result = quick_game.update(input, save);
            if(result.save_dirty)
            {
                tb::store_save(save);
            }
            if(result.exit)
            {
                ui.update(controller, input);
            }
        }
        else
        {
            const tb::UiUpdateResult result = controller.update(input, save);
            if(result.save_dirty)
            {
                tb::store_save(save);
            }

            if(controller.pending_game_request() == tb::GameRequest::QuickGame)
            {
                controller.clear_game_request();
                ui.hide();
                quick_game.start(controller.language());
            }
            else
            {
                ui.update(controller, input);
            }
        }

        bn::core::update();
    }
}
