#include "tb/gameplay_workers.h"

#include <cassert>

namespace tb
{
namespace
{
constexpr std::array<int, 7> path_y = {0, 5, 9, 12, 14, 15, 15};
constexpr std::array<int, 7> path_x = {0, 5, 11, 18, 26, 35, 45};
constexpr uint64_t visual_seed = 0x484F555345ULL; // "HOUSE": cosmetic-only deterministic seed.
}

GameplayWorkerField::GameplayWorkerField()
{
    reset();
}

void GameplayWorkerField::reset()
{
    _workers = {};
    _update_accumulator_ms = 0;
    _clock_ms = 0;
    _step_ready = false;
    _random_state = (visual_seed ^ _java_multiplier) & _java_mask;
}

bool GameplayWorkerField::begin_frame(int delta_ms)
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
        _step_ready = false;
        return false;
    }

    _clock_ms += _update_accumulator_ms;
    _update_accumulator_ms = 0;
    _step_ready = true;
    return true;
}

void GameplayWorkerField::finish_frame(const GameplayWorkerWorld& world)
{
    if(! _step_ready)
    {
        return;
    }

    for(GameplayWorker& worker : _workers)
    {
        if(worker.state != 0)
        {
            _update_worker(worker, world);
            _clip_worker(worker, world);
        }
    }
    _step_ready = false;
}

int GameplayWorkerField::spawn_for_landing(int absolute_offset, const GameplayWorkerWorld& world)
{
    const int wanted = _spawn_count_for_offset(absolute_offset);
    int spawned = 0;
    for(GameplayWorker& worker : _workers)
    {
        if(spawned >= wanted)
        {
            break;
        }
        if(worker.state == 0)
        {
            const int index = int(&worker - _workers.data());
            _spawn_one(world.floor_count, world);
            if(_workers[index].state != 0)
            {
                ++spawned;
            }
        }
    }
    return spawned;
}

void GameplayWorkerField::scatter_floor(
        int floor_number, int absolute_offset, const GameplayWorkerWorld& world)
{
    int scatter_budget = _spawn_count_for_offset(absolute_offset) >> 1;

    for(GameplayWorker& worker : _workers)
    {
        if(worker.state != 0 && worker.floor_number == floor_number)
        {
            worker.origin_x = worker.x_fixed;
            worker.origin_y = worker.y_fixed;
            worker.state = 3;
            const int previous_elapsed = _clock_ms - worker.state_started_ms;
            const int sign = worker.x_fixed < 0 ? -1 : 1;
            worker.floor_number = sign * (previous_elapsed / 500);
            worker.state_started_ms = _clock_ms;
            --scatter_budget;
        }
    }

    if(scatter_budget <= 0)
    {
        return;
    }

    int floor_x = 0;
    int floor_y = 0;
    if(! _floor_position(world, floor_number, floor_x, floor_y))
    {
        return;
    }

    for(GameplayWorker& worker : _workers)
    {
        if(scatter_budget <= 0)
        {
            break;
        }
        if(worker.state == 0)
        {
            // House.i(int) reuses the slot without touching b[10], so its
            // blue/red resource selection survives a scatter respawn.
            const int preserved_variant = worker.variant;
            worker = {};
            worker.variant = preserved_variant;
            worker.state_started_ms = _clock_ms;
            worker.state = 4;
            worker.floor_number = _next_random(4);
            worker.draw_direction = _next_random(2) == 0 ? -1 : 1;
            worker.origin_x = floor_x;
            worker.origin_y = floor_y;
            worker.x_fixed = floor_x;
            worker.y_fixed = floor_y;
            --scatter_budget;
        }
    }
}

const GameplayWorker& GameplayWorkerField::worker(int index) const
{
    assert(index >= 0 && index < worker_count);
    return _workers[index];
}

int GameplayWorkerField::active_count() const
{
    int result = 0;
    for(const GameplayWorker& worker : _workers)
    {
        if(worker.state != 0)
        {
            ++result;
        }
    }
    return result;
}

int GameplayWorkerField::clock_ms() const
{
    return _clock_ms;
}

int GameplayWorkerField::source_frame(const GameplayWorker& worker)
{
    if(worker.frame < 0 || worker.frame > 7)
    {
        return -1;
    }
    return worker.draw_direction < 0 ? 7 - worker.frame : worker.frame;
}

#ifdef TB_HOST_TEST
void GameplayWorkerField::debug_set_worker_for_test(int index, const GameplayWorker& worker)
{
    assert(index >= 0 && index < worker_count);
    _workers[index] = worker;
}

void GameplayWorkerField::debug_set_clock_for_test(int clock_ms)
{
    _clock_ms = clock_ms;
}

void GameplayWorkerField::debug_finish_frame_for_test(const GameplayWorkerWorld& world)
{
    for(GameplayWorker& worker : _workers)
    {
        if(worker.state != 0)
        {
            _update_worker(worker, world);
        }
    }
}
#endif

int GameplayWorkerField::_next_bits(int bits)
{
    _random_state = (_random_state * _java_multiplier + _java_addend) & _java_mask;
    return int(_random_state >> (48 - bits));
}

int GameplayWorkerField::_next_random(int bound)
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

int GameplayWorkerField::_abs(int value)
{
    return value < 0 ? -value : value;
}

int GameplayWorkerField::_spawn_count_for_offset(int absolute_offset)
{
    absolute_offset = _abs(absolute_offset);
    if(absolute_offset < 25)
    {
        return 4;
    }
    if(absolute_offset < 50)
    {
        return 3;
    }
    if(absolute_offset < 80)
    {
        return 2;
    }
    return 1;
}

bool GameplayWorkerField::_floor_position(
        const GameplayWorkerWorld& world, int floor_number, int& x, int& y)
{
    const int slot = floor_number - world.first_floor_number;
    if(slot < 0 || slot >= world.floor_slot_count)
    {
        return false;
    }
    x = world.floor_x[slot];
    y = world.floor_y[slot];
    return true;
}

void GameplayWorkerField::_spawn_one(int floor_number, const GameplayWorkerWorld& world)
{
    GameplayWorker* target = nullptr;
    for(GameplayWorker& worker : _workers)
    {
        if(worker.state == 0)
        {
            target = &worker;
            break;
        }
    }
    if(! target || floor_number <= 0)
    {
        return;
    }

    const int side = 1 - (2 * _next_random(2));
    target->state = 1;
    target->floor_number = floor_number;
    target->origin_x = world.camera_x + side * ((_view_width_fixed >> 1) + 256);
    target->target_x = world.tower_x;
    target->origin_y = world.camera_y + (_view_height_fixed >> 1) + _next_random(768);
    target->state_started_ms = _clock_ms - _next_random(1000);
    target->variant = _next_random(2);
    target->draw_direction = -side;
    target->walk_direction = 1;
    target->x_fixed = target->origin_x;
    target->y_fixed = target->origin_y;
}

void GameplayWorkerField::_update_worker(GameplayWorker& worker, const GameplayWorkerWorld& world)
{
    const int elapsed = _clock_ms - worker.state_started_ms;

    switch(worker.state)
    {
    case 1:
    {
        int segment = elapsed / 500;
        int remainder = elapsed - segment * 500;
        if(segment > 5)
        {
            segment = 5;
            remainder = 500;
        }
        if(segment < 0)
        {
            segment = 0;
            remainder = 0;
        }

        const int target_y = (worker.floor_number - 1) * 256 + 128;
        const int x_distance = worker.origin_x - worker.target_x;
        const int y_distance = worker.origin_y - target_y;
        worker.x_fixed = worker.origin_x - path_x[segment] * x_distance / 45 -
                remainder * (path_x[segment + 1] - path_x[segment]) * x_distance / 22500;
        worker.y_fixed = worker.origin_y - path_y[segment] * y_distance / 15 -
                remainder * (path_y[segment + 1] - path_y[segment]) * y_distance / 7500;

        worker.frame = 1 + ((elapsed - 1200) / 280) % 8;
        if(worker.frame > 5)
        {
            worker.frame = 5 - (worker.frame - 5);
        }

        if(elapsed >= 2000)
        {
            int floor_x = 0;
            int floor_y = 0;
            if(_floor_position(world, worker.floor_number, floor_x, floor_y) &&
               _abs(worker.x_fixed - floor_x) < 128 && worker.y_fixed <= target_y)
            {
                const int relative_x = worker.x_fixed - floor_x;
                const int relative_y = target_y - worker.y_fixed;
                worker.origin_x = relative_x;
                worker.origin_y = relative_y;
                worker.state = 2;
                worker.state_started_ms = _clock_ms;
                worker.walk_direction = relative_x < 0 ? -1 : 1;
                worker.draw_direction = 1;
                worker.x_fixed = floor_x + relative_x;
                worker.y_fixed = floor_y - relative_y;
            }
        }
        break;
    }

    case 2:
    {
        int floor_x = 0;
        int floor_y = 0;
        if(! _floor_position(world, worker.floor_number, floor_x, floor_y))
        {
            worker.state = 0;
            break;
        }

        worker.x_fixed = floor_x + worker.origin_x - worker.walk_direction * elapsed / 12;
        if(_abs(worker.x_fixed - floor_x) < 64)
        {
            worker.x_fixed = floor_x + worker.walk_direction * 64;
        }
        worker.y_fixed = floor_y + (floor_y - worker.origin_y) * elapsed / 12;
        if(worker.y_fixed > floor_y)
        {
            worker.y_fixed = floor_y;
        }

        if(worker.x_fixed == floor_x + worker.walk_direction * 64 && worker.y_fixed == floor_y)
        {
            worker.origin_x = worker.walk_direction * 64;
            worker.state = 5;
            worker.state_started_ms = _clock_ms;
        }
        worker.frame = 6 + ((_clock_ms / 200) % 2);
        break;
    }

    case 3:
    {
        const int velocity = worker.floor_number;
        worker.x_fixed = worker.origin_x - velocity * elapsed / 30;
        worker.y_fixed = worker.origin_y - _abs(10 - velocity) * elapsed / 30;
        worker.frame = 1 + (elapsed / 280) % 8;
        if(worker.frame > 5)
        {
            worker.frame = 5 - (worker.frame - 5);
        }
        break;
    }

    case 4:
    {
        const int speed = worker.floor_number;
        const int direction = worker.draw_direction;
        worker.x_fixed = worker.origin_x - direction * (10 - speed) * elapsed / 15;
        worker.y_fixed = worker.origin_y + direction * speed * elapsed / 15;
        if(elapsed > 300)
        {
            worker.origin_x = worker.x_fixed;
            worker.origin_y = worker.y_fixed;
            worker.state = 3;
            worker.state_started_ms = _clock_ms;
        }
        worker.frame = 0;
        break;
    }

    case 5:
    {
        int floor_x = 0;
        int floor_y = 0;
        if(! _floor_position(world, worker.floor_number, floor_x, floor_y))
        {
            worker.state = 0;
            break;
        }
        worker.x_fixed = floor_x + worker.origin_x;
        worker.y_fixed = floor_y;
        if(elapsed > 500)
        {
            worker.state = 0;
        }
        worker.frame = 7;
        break;
    }

    default:
        worker.state = 0;
        break;
    }
}

void GameplayWorkerField::_clip_worker(GameplayWorker& worker, const GameplayWorkerWorld& world)
{
    if(worker.state == 0)
    {
        return;
    }

    if(worker.y_fixed < world.camera_y - (_view_height_fixed >> 1) ||
       worker.x_fixed < world.camera_x - (_view_width_fixed >> 1) - 256 ||
       worker.x_fixed > world.camera_x + (_view_width_fixed >> 1) + 256)
    {
        worker.state = 0;
    }
}
}
