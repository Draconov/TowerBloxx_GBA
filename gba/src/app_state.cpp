#include "tb/app_state.h"

namespace tb
{
SceneId AppState::scene() const
{
    return _scene;
}

void AppState::set_scene(SceneId scene)
{
    _scene = scene;
}

InputFrame AppState::update_input(uint16_t held_mask)
{
    InputFrame result;
    result.held_mask = held_mask;
    result.pressed_mask = static_cast<uint16_t>(held_mask & ~_previous_held_mask);
    _previous_held_mask = held_mask;
    return result;
}
}
