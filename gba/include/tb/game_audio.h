#ifndef TB_GAME_AUDIO_H
#define TB_GAME_AUDIO_H

#include <cstdint>

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
    void update(bool enabled, AudioScene scene);
    void play_construction_result(uint8_t roof);

private:
    void _play_scene(AudioScene scene);

    AudioScene _scene = AudioScene::Menu;
    bool _enabled = false;
    bool _started = false;
};
}

#endif
