#ifndef TB_MENU_WORKERS_H
#define TB_MENU_WORKERS_H

#include <cstdint>

namespace tb
{
struct MenuWorker
{
    int x_fixed = 0;
    int y_fixed = 0;
    int animation_state = 0;
    int vx = 0;
    int vy = 0;
    int variant = 0;
};

class MenuWorkerField
{
public:
    static constexpr int worker_count = 3;

    MenuWorkerField();

    void reset();
    [[nodiscard]] bool update(int delta_ms);
    [[nodiscard]] const MenuWorker& worker(int index) const;
    [[nodiscard]] int height_fixed() const;
    [[nodiscard]] static int display_frame(int animation_state);

private:
    [[nodiscard]] int _next_bits(int bits);
    [[nodiscard]] int _next_random(int bound);
    void _respawn(MenuWorker& worker);

    static constexpr int _width_fixed = (256 * 240) / 22;
    static constexpr int _height_fixed = (256 * 160) / 22;
    static constexpr uint64_t _java_multiplier = 0x5DEECE66DULL;
    static constexpr uint64_t _java_addend = 0xBULL;
    static constexpr uint64_t _java_mask = (1ULL << 48) - 1;

    MenuWorker _workers[worker_count]{};
    int _update_accumulator_ms = 0;
    int _animation_timer_ms = 0;
    uint64_t _random_state = 0;
};
}

#endif
