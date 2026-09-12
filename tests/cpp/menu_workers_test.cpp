#include <cassert>
#include <iostream>

#include "tb/menu_workers.h"

int main()
{
    static_assert(tb::MenuWorkerField::worker_count == 3);
    constexpr int expected_frames[] = {1, 2, 3, 4, 5, 4, 3, 2};
    for(int state = 0; state < 8; ++state)
    {
        assert(tb::MenuWorkerField::display_frame(state) == expected_frames[state]);
    }

    tb::MenuWorkerField field;
    const int height_fixed = field.height_fixed();
    for(int index = 0; index < tb::MenuWorkerField::worker_count; ++index)
    {
        assert(field.worker(index).y_fixed == height_fixed);
    }

    // House.c(int,int) ignores sub-25ms accumulated time.
    assert(! field.update(16));
    assert(field.worker(0).y_fixed == height_fixed);

    // The next 17ms update crosses the gate, respawns all workers above the
    // viewport and gives them a positive downward velocity in [10, 19].
    assert(field.update(17));
    for(int index = 0; index < tb::MenuWorkerField::worker_count; ++index)
    {
        const auto& worker = field.worker(index);
        assert(worker.y_fixed <= -267);
        assert(worker.y_fixed > -267 - height_fixed);
        assert(worker.vy >= 10 && worker.vy <= 19);
        assert(worker.vx >= -8 && worker.vx <= 7);
        assert(worker.variant == 0 || worker.variant == 1);
    }

    const int before_y = field.worker(0).y_fixed;
    assert(field.update(150));
    assert(field.worker(0).y_fixed > before_y);

    // Delta is clamped to 150ms: two identical fresh fields must land on the
    // same state for 150ms and an oversized 1000ms request.
    tb::MenuWorkerField clamped_a;
    tb::MenuWorkerField clamped_b;
    assert(clamped_a.update(25));
    assert(clamped_b.update(25));
    assert(clamped_a.update(150));
    assert(clamped_b.update(1000));
    for(int index = 0; index < tb::MenuWorkerField::worker_count; ++index)
    {
        const auto& a = clamped_a.worker(index);
        const auto& b = clamped_b.worker(index);
        assert(a.x_fixed == b.x_fixed);
        assert(a.y_fixed == b.y_fixed);
        assert(a.animation_state == b.animation_state);
    }

    std::cout << "menu workers ok\n";
}
