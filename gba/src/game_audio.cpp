#include "tb/game_audio.h"

#include "bn_jingle.h"
#include "bn_music.h"
#include "bn_music_items.h"

namespace tb
{
void GameAudio::update(bool enabled, AudioScene scene)
{
    if(! enabled)
    {
        if(_enabled || _started)
        {
            bn::music::stop();
            bn::jingle::stop();
        }
        _enabled = false;
        _started = false;
        _scene = scene;
        return;
    }

    if(! _enabled || ! _started || scene != _scene)
    {
        _enabled = true;
        _scene = scene;
        _play_scene(scene);
        _started = true;
    }
}

void GameAudio::play_construction_result(uint8_t roof)
{
    if(! _enabled)
    {
        return;
    }

    if(roof == 2)
    {
        bn::music_items::trophy_roof.play_jingle(0.5);
    }
    else if(roof == 1)
    {
        bn::music_items::normal_roof.play_jingle(0.5);
    }
    else
    {
        bn::music_items::construction_fail.play_jingle(0.5);
    }
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
