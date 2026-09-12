#include "tb/menu_workers.h"

#include <cassert>

namespace tb
{
namespace
{
constexpr uint64_t visual_seed = 0x54424C4FULL; // "TBLO"; cosmetic and deterministic.
}

MenuWorkerField::MenuWorkerField()
{
    reset();
}

void MenuWorkerField::reset()
{
    _update_accumulator_ms = 0;
    _animation_timer_ms = 0;
    _random_state = (visual_seed ^ _java_multiplier) & _java_mask;
    for(MenuWorker& worker : _workers)
    {
        worker = {};
        worker.y_fixed = _height_fixed;
    }
}

bool MenuWorkerField::update(int delta_ms)
{
    if(delta_ms < 0)
    {
        delta_ms = 0;
    }
    if(delta_ms > 150)
    {
        delta_ms = 150;
    }

    _update_accumulator_ms += delta_ms;
    if(_update_accumulator_ms < 25)
    {
        return false;
    }

    const int elapsed_ms = _update_accumulator_ms;
    _update_accumulator_ms = 0;
    _animation_timer_ms -= elapsed_ms;
    if(_animation_timer_ms < 0)
    {
        _animation_timer_ms = 300;
        for(MenuWorker& worker : _workers)
        {
            worker.animation_state = (worker.animation_state + 1) % 8;
        }
    }

    for(MenuWorker& worker : _workers)
    {
        worker.x_fixed += (worker.vx * elapsed_ms) >> 6;
        worker.y_fixed += (worker.vy * elapsed_ms) >> 6;
        if(worker.y_fixed >= _height_fixed)
        {
            _respawn(worker);
        }
    }
    return true;
}

const MenuWorker& MenuWorkerField::worker(int index) const
{
    assert(index >= 0 && index < worker_count);
    return _workers[index];
}

int MenuWorkerField::height_fixed() const
{
    return _height_fixed;
}

int MenuWorkerField::display_frame(int animation_state)
{
    int frame = (animation_state & 7) + 1;
    if(frame > 5)
    {
        frame = 5 - (frame - 5);
    }
    return frame;
}

int MenuWorkerField::_next_bits(int bits)
{
    _random_state = (_random_state * _java_multiplier + _java_addend) & _java_mask;
    return int(_random_state >> (48 - bits));
}

int MenuWorkerField::_next_random(int bound)
{
    assert(bound > 0);
    if((bound & -bound) == bound)
    {
        return int((int64_t(bound) * _next_bits(31)) >> 31);
    }

    int bits;
    int value;
    do
    {
        bits = _next_bits(31);
        value = bits % bound;
    }
    while(int64_t(bits) - value + (bound - 1) >= (int64_t(1) << 31));
    return value;
}

void MenuWorkerField::_respawn(MenuWorker& worker)
{
    worker.x_fixed = _next_random(_width_fixed);
    worker.y_fixed = -267 - _next_random(_height_fixed);
    worker.animation_state = _next_random(8);
    worker.vx = _next_random(16) - 8;
    worker.vy = 10 + _next_random(10);
    worker.variant = _next_random(2);
}
}
