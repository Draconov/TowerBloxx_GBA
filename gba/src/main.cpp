#include "bn_core.h"
#include "bn_keypad.h"

#include "tb/app_state.h"
#include "tb/build_city_scene.h"
#include "tb/game_audio.h"
#include "tb/quick_game_scene.h"
#include "tb/save_store.h"
#include "tb/tower_construction_scene.h"
#include "tb/tower_session.h"
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
    tb::SaveData sandbox_save = save; // Volatile city copy; never written to SRAM.
    tb::UiController controller(save);
    tb::UiShell ui;
    tb::QuickGameScene quick_game;
    tb::BuildCityScene build_city(save);
    tb::TowerConstructionScene construction;
    tb::TowerSessionCoordinator session;
    tb::GameAudio audio;

    // Butano releases sprite tile VRAM on bn::core::update(), not immediately
    // when sprite_ptr instances are destroyed.  Returning from construction
    // directly into the Build City rebuild in the same frame retains the old
    // tower graphics in VRAM while the city HUD allocates text tiles.
    bool city_resume_pending = false;

    while(true)
    {
        const tb::InputFrame input = app.update_input(held_keys());
        controller.set_suspended_session_available(session.has_suspended());

        switch(session.foreground())
        {
        case tb::RuntimeScene::QuickGame:
        {
            const tb::QuickGameSceneUpdateResult result = quick_game.update(input, save);
            if(result.save_dirty)
            {
                tb::store_save(save);
            }

            if(result.suspend_requested)
            {
                quick_game.suspend_presentation();
                session.suspend_quick_game();
            }
            else if(result.score_ready)
            {
                quick_game.discard();
                session.show_ui();
                controller.set_suspended_session_available(false);
                controller.begin_score_submission(
                        tb::HallTable::QuickGame,
                        result.final_population,
                        save,
                        tb::ScoreFlowReturn::RootMenu);
            }
            else if(result.exit)
            {
                quick_game.discard();
                session.show_ui();
            }
            break;
        }

        case tb::RuntimeScene::BuildCity:
        {
            tb::SaveData& city_save = build_city.sandbox_active() ? sandbox_save : save;
            if(city_resume_pending)
            {
                // The construction scene was discarded during the previous
                // frame, so bn::core::update() has now reclaimed its OBJ tiles.
                // Only recreate city graphics after that reclamation.
                build_city.resume_presentation(city_save);
                city_resume_pending = false;
            }
            const tb::BuildCitySceneUpdateResult result = build_city.update(input, city_save);
            if(result.sandbox_activated)
            {
                sandbox_save = save;
            }
            if(result.save_dirty && ! build_city.sandbox_active())
            {
                tb::store_save(save);
            }

            if(result.construction_requested)
            {
                const tb::BuildCityConstructionRequest request = build_city.construction_request();
                if(request.pending)
                {
                    build_city.clear_construction_request();
                    construction.start(request, controller.language(),
                                       build_city.sandbox_active() ? sandbox_save : save);
                    session.start_construction();
                }
            }
            else if(result.placement_committed)
            {
                const auto score_submission = controller.begin_score_submission(
                        tb::HallTable::BuildCity,
                        uint32_t(result.committed_total_population),
                        save,
                        tb::ScoreFlowReturn::BuildCity);
                if(score_submission.save_dirty)
                {
                    tb::store_save(save);
                }
                if(score_submission.requires_ui)
                {
                    build_city.suspend_presentation();
                    session.show_ui();
                }
            }
            else if(result.exit)
            {
                session.show_ui();
            }
            break;
        }

        case tb::RuntimeScene::Construction:
        {
            tb::SaveData& construction_save = build_city.sandbox_active() ? sandbox_save : save;
            const tb::TowerConstructionSceneUpdateResult result = construction.update(input, construction_save);
            if(result.save_dirty && ! build_city.sandbox_active())
            {
                tb::store_save(save);
            }
            if(result.suspend_requested)
            {
                construction.suspend_presentation();
                session.suspend_construction();
            }
            else if(result.completed)
            {
                build_city.accept_constructed_tower(result.building_type, result.population, result.roof,
                                                   construction_save);
                // Defer the city rebuild until the following frame.  A trophy
                // result can leave the old construction graphics occupying OBJ
                // VRAM until bn::core::update() processes their destruction.
                city_resume_pending = true;
                audio.play_construction_result(result.roof);
                session.return_to_build_city();
            }
            else if(result.exit)
            {
                city_resume_pending = true;
                session.return_to_build_city();
            }
            break;
        }

        case tb::RuntimeScene::Ui:
        {
            const tb::UiUpdateResult result = controller.update(input, save);
            if(result.save_dirty)
            {
                tb::store_save(save);
            }

            switch(result.action)
            {
            case tb::UiAction::StartQuickGame:
            case tb::UiAction::StartBuildCity:
            {
                if(session.has_suspended())
                {
                    if(session.suspended_kind() == tb::SuspendedSessionKind::QuickGame)
                    {
                        quick_game.discard();
                    }
                    else if(session.suspended_kind() == tb::SuspendedSessionKind::BuildCityConstruction)
                    {
                        construction.discard();
                    }
                    session.discard_suspended();
                }

                ui.hide();
                if(result.action == tb::UiAction::StartQuickGame)
                {
                    quick_game.start(controller.language());
                    session.start_quick_game();
                }
                else
                {
                    build_city.start(save, controller.language());
                    session.start_build_city();
                }
                break;
            }

            case tb::UiAction::ResumeSuspended:
            {
                const tb::SuspendedSessionKind kind = session.resume_suspended();
                ui.hide();
                if(kind == tb::SuspendedSessionKind::QuickGame)
                {
                    quick_game.resume_presentation();
                }
                else if(kind == tb::SuspendedSessionKind::BuildCityConstruction)
                {
                    construction.resume_presentation();
                }
                break;
            }

            case tb::UiAction::ReturnToBuildCity:
                ui.hide();
                build_city.resume_presentation(save);
                session.return_to_build_city();
                break;

            case tb::UiAction::None:
                ui.update(controller, save, input);
                break;
            }
            break;
        }
        }

        tb::AudioScene audio_scene = tb::AudioScene::Menu;
        if(session.foreground() == tb::RuntimeScene::QuickGame ||
           session.foreground() == tb::RuntimeScene::Construction)
        {
            audio_scene = tb::AudioScene::Tower;
        }
        else if(session.foreground() == tb::RuntimeScene::BuildCity)
        {
            audio_scene = tb::AudioScene::City;
        }
        audio.update(save.sound_enabled != 0, audio_scene);

        bn::core::update();
    }
}
