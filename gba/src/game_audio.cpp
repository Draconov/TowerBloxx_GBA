#include "tb/game_audio.h"

#include "bn_music.h"
#include "bn_music_items.h"

namespace tb
{
void GameAudio::update(bool enabled, AudioScene scene, VisualTheme theme)
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
        _theme = theme;
        return;
    }

    const bool was_enabled = _enabled;
    const bool scene_changed = scene != _scene;
    const bool theme_changed = theme != _theme;
    _enabled = true;
    _scene = scene;
    _theme = theme;

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

    if(! was_enabled || ! _started || scene_changed || theme_changed)
    {
        _play_scene(_scene, _theme);
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

void GameAudio::_play_scene(AudioScene scene, VisualTheme theme)
{
    bn::music::stop();
    const bool christmas = theme == VisualTheme::Christmas;
    switch(scene)
    {
    case AudioScene::Menu:
        if(christmas)
        {
            bn::music_items::christmas_menu_theme.play(0.5, true);
        }
        else
        {
            bn::music_items::menu_theme.play(0.5, true);
        }
        break;
    case AudioScene::Tower:
        if(christmas)
        {
            bn::music_items::christmas_tower_theme.play(0.5, true);
        }
        else
        {
            bn::music_items::tower_theme.play(0.5, true);
        }
        break;
    case AudioScene::City:
        if(christmas)
        {
            bn::music_items::christmas_city_theme.play(0.5, true);
        }
        else
        {
            bn::music_items::city_theme.play(0.5, true);
        }
        break;
    }
}
}
