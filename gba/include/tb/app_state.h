#ifndef TB_APP_STATE_H
#define TB_APP_STATE_H

#include <cstdint>

namespace tb
{
enum class SceneId : uint8_t
{
    UiShell = 0,
    TowerGallery,
};

enum class Key : uint16_t
{
    A = 1u << 0,
    B = 1u << 1,
    Select = 1u << 2,
    Start = 1u << 3,
    Right = 1u << 4,
    Left = 1u << 5,
    Up = 1u << 6,
    Down = 1u << 7,
};

constexpr uint16_t key_mask(Key key)
{
    return static_cast<uint16_t>(key);
}

struct InputFrame
{
    uint16_t held_mask = 0;
    uint16_t pressed_mask = 0;

    [[nodiscard]] constexpr bool held(Key key) const
    {
        return (held_mask & key_mask(key)) != 0;
    }

    [[nodiscard]] constexpr bool pressed(Key key) const
    {
        return (pressed_mask & key_mask(key)) != 0;
    }
};

class AppState
{
public:
    [[nodiscard]] SceneId scene() const;
    void set_scene(SceneId scene);
    [[nodiscard]] InputFrame update_input(uint16_t held_mask);

private:
    SceneId _scene = SceneId::UiShell;
    uint16_t _previous_held_mask = 0;
};
}

#endif
