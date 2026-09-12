#include "bn_core.h"
#include "bn_keypad.h"

#include "tb/app_state.h"
#include "tb/build_city_scene.h"
#include "tb/game_audio.h"
#include "tb/quick_game_scene.h"
#include "tb/save_store.h"
#include "tb/tower_construction_scene.h"
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
    tb::BuildCityScene build_city(save);
    tb::TowerConstructionScene construction;
    tb::GameAudio audio;

    while(true)
    {
        const tb::InputFrame input = app.update_input(held_keys());

        if(construction.active())
        {
            const tb::TowerConstructionSceneUpdateResult result = construction.update(input);
            if(result.completed)
            {
                build_city.accept_constructed_tower(result.building_type, result.population, result.roof);
                audio.play_construction_result(result.roof);
            }
        }
        else if(quick_game.active())
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
        else if(build_city.active())
        {
            const tb::BuildCitySceneUpdateResult result = build_city.update(input, save);
            if(result.save_dirty)
            {
                tb::store_save(save);
            }
            if(result.construction_requested)
            {
                const tb::BuildCityConstructionRequest request = build_city.construction_request();
                if(request.pending)
                {
                    build_city.clear_construction_request();
                    construction.start(request, controller.language());
                }
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
            else if(controller.pending_game_request() == tb::GameRequest::BuildCity)
            {
                controller.clear_game_request();
                ui.hide();
                build_city.start(save, controller.language());
            }
            else
            {
                ui.update(controller, input);
            }
        }

        tb::AudioScene audio_scene = tb::AudioScene::Menu;
        if(construction.active() || quick_game.active())
        {
            audio_scene = tb::AudioScene::Tower;
        }
        else if(build_city.active())
        {
            audio_scene = tb::AudioScene::City;
        }
        audio.update(save.sound_enabled != 0, audio_scene);

        bn::core::update();
    }
}
