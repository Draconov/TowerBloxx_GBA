#include <cassert>
#include <iostream>

#include "tb/gameplay_workers.h"

namespace
{
tb::GameplayWorkerWorld single_floor_world()
{
    tb::GameplayWorkerWorld world;
    world.camera_x = 0;
    world.camera_y = 512;
    world.tower_x = 0;
    world.floor_count = 1;
    world.first_floor_number = 1;
    world.floor_slot_count = 1;
    world.floor_x[0] = 0;
    world.floor_y[0] = 128;
    return world;
}
}

int main()
{
    static_assert(tb::GameplayWorkerField::worker_count == 8);

    tb::GameplayWorkerField field;
    for(int index = 0; index < tb::GameplayWorkerField::worker_count; ++index)
    {
        assert(field.worker(index).state == 0);
    }

    // House.d(count, floor) spawns 4/3/2/1 workers at the recovered
    // alignment bands <25, <50, <80, >=80.
    const auto world = single_floor_world();
    assert(field.spawn_for_landing(0, world) == 4);
    assert(field.active_count() == 4);

    tb::GameplayWorkerField great;
    assert(great.spawn_for_landing(25, world) == 3);
    tb::GameplayWorkerField good;
    assert(good.spawn_for_landing(50, world) == 2);
    tb::GameplayWorkerField ok;
    assert(ok.spawn_for_landing(80, world) == 1);

    // Gameplay workers are updated by the same >=25 ms House gate.
    tb::GameplayWorkerField gated;
    assert(! gated.begin_frame(16));
    assert(gated.clock_ms() == 0);
    assert(gated.begin_frame(17));
    assert(gated.clock_ms() == 33);

#ifdef TB_HOST_TEST
    // Pin the recovered state-1 curve directly. House uses:
    // x curve r={0,5,11,18,26,35,45}
    // y curve q={0,5,9,12,14,15,15}.
    tb::GameplayWorkerField curve;
    tb::GameplayWorker worker;
    worker.state = 1;
    worker.origin_x = 1000;
    worker.origin_y = 2000;
    worker.target_x = 0;
    worker.floor_number = 1;
    worker.state_started_ms = 0;
    worker.draw_direction = 1;
    curve.debug_set_worker_for_test(0, worker);
    curve.debug_set_clock_for_test(1500);
    curve.debug_finish_frame_for_test(world);
    const auto& curved = curve.worker(0);
    assert(curved.x_fixed == 600);
    assert(curved.y_fixed == 503);
    assert(curved.frame == 2);

    // The Java draw helper reverses source frame numbers rather than
    // horizontally flipping the sprite when direction is negative.
    tb::GameplayWorker facing;
    facing.frame = 2;
    facing.draw_direction = 1;
    assert(tb::GameplayWorkerField::source_frame(facing) == 2);
    facing.draw_direction = -1;
    assert(tb::GameplayWorkerField::source_frame(facing) == 5);
    facing.frame = -1;
    assert(tb::GameplayWorkerField::source_frame(facing) == -1);

    // House.i(floor) does not rewrite b[10] when it reuses an inactive
    // slot for a thrown/scattered worker; the blue/red variant persists.
    tb::GameplayWorkerField scatter;
    tb::GameplayWorker reused;
    reused.state = 0;
    reused.variant = 1;
    scatter.debug_set_worker_for_test(0, reused);
    scatter.scatter_floor(1, 0, world);
    assert(scatter.worker(0).state == 4);
    assert(scatter.worker(0).variant == 1);
#endif

    std::cout << "gameplay workers ok\n";
}
