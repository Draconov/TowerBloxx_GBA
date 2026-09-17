#include "tb/menu_clouds.h"

#include <cassert>

namespace tb
{
namespace
{
constexpr uint64_t visual_seed = 0x434C4F5544ULL; // "CLOUD", cosmetic-only deterministic seed.
constexpr int cloud_bottom_margin_fixed = (256 * 32) / 22;
constexpr int cloud_top_margin_fixed = (256 * 28) / 22;
}

MenuCloudField::MenuCloudField()
{
    reset();
}

void MenuCloudField::reset()
{
    _update_accumulator_ms = 0;
    _random_state = (visual_seed ^ _java_multiplier) & _java_mask;
    for(MenuCloud& cloud : _clouds)
    {
        _spawn(cloud, true);
    }
}

bool MenuCloudField::update(int delta_ms)
{
    if(delta_ms < 0)
    {
        delta_ms = 0;
    }
    else if(delta_ms > 150)
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
    for(MenuCloud& cloud : _clouds)
    {
        cloud.y_fixed += (cloud.vy * elapsed_ms) >> 6;
        if(cloud.y_fixed >= _height_fixed + cloud_bottom_margin_fixed)
        {
            _spawn(cloud, false);
        }
    }
    return true;
}

const MenuCloud& MenuCloudField::cloud(int index) const
{
    assert(index >= 0 && index < cloud_count);
    return _clouds[index];
}

int MenuCloudField::_next_bits(int bits)
{
    _random_state = (_random_state * _java_multiplier + _java_addend) & _java_mask;
    return int(_random_state >> (48 - bits));
}

int MenuCloudField::_next_random(int bound)
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

void MenuCloudField::_spawn(MenuCloud& cloud, bool initial)
{
    cloud.type = _next_random(2);
    cloud.vx = 0;
    cloud.vy = cloud.type == 0 ? 6 : 9;

    // The source lets clouds begin partly outside the horizontal viewport.
    const int side_margin_fixed = cloud.type == 0 ? (256 * 28) / 22 : (256 * 54) / 22;
    cloud.x_fixed = _next_random(_width_fixed + side_margin_fixed * 2) - side_margin_fixed;

    if(initial)
    {
        cloud.y_fixed = _next_random(_height_fixed + cloud_top_margin_fixed * 2) - cloud_top_margin_fixed;
    }
    else
    {
        cloud.y_fixed = -cloud_top_margin_fixed - _next_random(_height_fixed);
    }
}
}
