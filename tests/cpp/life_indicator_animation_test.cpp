#include <cassert>
#include <iostream>

#include "tb/life_indicator_animation.h"

int main()
{
    tb::LifeIndicatorAnimation animation;
    animation.reset(3);

    // Quick Game orange family starts live on frame 6.
    for(int slot = 0; slot < 3; ++slot)
    {
        assert(animation.frame_for_slot(slot, 3, 6) == 6);
    }

    // First miss: top visible cell shows the matching broken frame for 100 ms.
    assert(animation.advance(16, 2));
    assert(animation.frame_for_slot(0, 2, 6) == 7);
    assert(animation.frame_for_slot(1, 2, 6) == 6);
    assert(animation.frame_for_slot(2, 2, 6) == 6);
    animation.advance(99, 2);
    assert(animation.frame_for_slot(0, 2, 6) == 7);
    animation.advance(1, 2);
    assert(animation.frame_for_slot(0, 2, 6) == 8);

    // Second miss uses the same broken frame and leaves one warning cell.
    animation.advance(16, 1);
    assert(animation.frame_for_slot(1, 1, 6) == 7);
    const int warning_now = animation.frame_for_slot(2, 1, 6);
    assert(warning_now == 6 || warning_now == 9);

    // The one remaining chance alternates with source frame 9 every 500 ms.
    const int before_phase = animation.frame_for_slot(2, 1, 6);
    animation.advance(500, 1);
    const int after_phase = animation.frame_for_slot(2, 1, 6);
    assert(before_phase != after_phase);
    assert((before_phase == 9 && after_phase == 6) || (before_phase == 6 && after_phase == 9));

    // The mapping is shared by construction families: blue 0->1, red 2->3,
    // green 4->5 and orange/yellow 6->7.
    tb::LifeIndicatorAnimation blue;
    blue.reset(3);
    blue.advance(16, 2);
    assert(blue.frame_for_slot(0, 2, 0) == 1);

    tb::LifeIndicatorAnimation red;
    red.reset(3);
    red.advance(16, 2);
    assert(red.frame_for_slot(0, 2, 2) == 3);

    tb::LifeIndicatorAnimation green;
    green.reset(3);
    green.advance(16, 2);
    assert(green.frame_for_slot(0, 2, 4) == 5);

    std::cout << "life indicator animation ok\n";
}
