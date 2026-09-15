#include "tb/game_audio.h"

#include "bn_music.h"
#include "bn_music_items.h"

namespace tb
{
void GameAudio::update(bool enabled, AudioScene scene)
{
    if(! enabled)
    {
        if(_enabled || _started || _result_active)
        {
            bn::music::stop();
        }
        _enabled = false;
        _started = false;
        _result_active = false;
        _scene = scene;
        return;
    }

    const bool was_enabled = _enabled;
    const bool scene_changed = scene != _scene;
    _enabled = true;
    _scene = scene;

    // o.playerUpdate(): a finite result track owns the single J2ME player.
    // Scene changes are remembered while it plays, but cannot interrupt it.
    if(_result_active)
    {
        if(bn::music::playing())
        {
            return;
        }
        _result_active = false;
        _started = false;
    }

    if(! was_enabled || ! _started || scene_changed)
    {
        _play_scene(_scene);
        _started = true;
    }
}

void GameAudio::play_construction_result(uint8_t roof)
{
    if(! _enabled)
    {
        return;
    }

    // House stops resource 38 before starting the finite 40/41/42 player.
    bn::music::stop();
    if(roof == 2)
    {
        bn::music_items::trophy_roof.play(0.5, false);
    }
    else if(roof == 1)
    {
        bn::music_items::normal_roof.play(0.5, false);
    }
    else
    {
        bn::music_items::construction_fail.play(0.5, false);
    }
    _result_active = true;
    _started = false;
}

void GameAudio::_play_scene(AudioScene scene)
{
    bn::music::stop();
    switch(scene)
    {
    case AudioScene::Menu:
        bn::music_items::menu_theme.play(0.5, true);
        break;
    case AudioScene::Tower:
        bn::music_items::tower_theme.play(0.5, true);
        break;
    case AudioScene::City:
        bn::music_items::city_theme.play(0.5, true);
        break;
    }
}
}
