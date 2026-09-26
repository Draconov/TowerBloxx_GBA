#ifndef TB_GAME_AUDIO_H
#define TB_GAME_AUDIO_H

#include <cstdint>

#include "tb/visual_theme.h"

namespace tb
{
enum class AudioScene : uint8_t
{
    Menu = 0,
    Tower,
    City,
};

class GameAudio
{
public:
    void update(bool enabled, AudioScene scene, VisualTheme theme);
    void play_construction_result(uint8_t roof);

private:
    void _play_scene(AudioScene scene, VisualTheme theme);

    AudioScene _scene = AudioScene::Menu;
    VisualTheme _theme = VisualTheme::Classic;
    bool _enabled = false;
    bool _started = false;
    bool _result_active = false;
};
}

#endif
