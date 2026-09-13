#ifndef TB_GAMEPLAY_WORKERS_H
#define TB_GAMEPLAY_WORKERS_H

#include <array>
#include <cstdint>

namespace tb
{
struct GameplayWorker
{
    int state = 0;
    int origin_x = 0;
    int origin_y = 0;
    int frame = 0;
    int state_started_ms = 0;
    int floor_number = 0;
    int draw_direction = 1;
    int x_fixed = 0;
    int y_fixed = 0;
    int target_x = 0;
    int variant = 0;
    int walk_direction = 1;
};

struct GameplayWorkerWorld
{
    static constexpr int max_floor_slots = 5;

    int camera_x = 0;
    int camera_y = 512;
    int tower_x = 0;
    int floor_count = 0;
    int first_floor_number = 1;
    int floor_slot_count = 0;
    std::array<int, max_floor_slots> floor_x{};
    std::array<int, max_floor_slots> floor_y{};
};

class GameplayWorkerField
{
public:
    static constexpr int worker_count = 8;

    GameplayWorkerField();

    void reset();
    [[nodiscard]] bool begin_frame(int delta_ms);
    void finish_frame(const GameplayWorkerWorld& world);
    [[nodiscard]] int spawn_for_landing(int absolute_offset, const GameplayWorkerWorld& world);
    void scatter_floor(int floor_number, int absolute_offset, const GameplayWorkerWorld& world);

    [[nodiscard]] const GameplayWorker& worker(int index) const;
    [[nodiscard]] int active_count() const;
    [[nodiscard]] int clock_ms() const;
    [[nodiscard]] static int source_frame(const GameplayWorker& worker);

#ifdef TB_HOST_TEST
    void debug_set_worker_for_test(int index, const GameplayWorker& worker);
    void debug_set_clock_for_test(int clock_ms);
    void debug_finish_frame_for_test(const GameplayWorkerWorld& world);
#endif

private:
    [[nodiscard]] int _next_bits(int bits);
    [[nodiscard]] int _next_random(int bound);
    [[nodiscard]] static int _abs(int value);
    [[nodiscard]] static int _spawn_count_for_offset(int absolute_offset);
    [[nodiscard]] static bool _floor_position(
            const GameplayWorkerWorld& world, int floor_number, int& x, int& y);
    void _spawn_one(int floor_number, const GameplayWorkerWorld& world);
    void _update_worker(GameplayWorker& worker, const GameplayWorkerWorld& world);
    void _clip_worker(GameplayWorker& worker, const GameplayWorkerWorld& world);

    static constexpr int _view_width_fixed = (256 * 240) / 22;
    static constexpr int _view_height_fixed = (256 * 160) / 22;
    static constexpr uint64_t _java_multiplier = 0x5DEECE66DULL;
    static constexpr uint64_t _java_addend = 0xBULL;
    static constexpr uint64_t _java_mask = (1ULL << 48) - 1;

    std::array<GameplayWorker, worker_count> _workers{};
    int _update_accumulator_ms = 0;
    int _clock_ms = 0;
    bool _step_ready = false;
    uint64_t _random_state = 0;
};
}

#endif
