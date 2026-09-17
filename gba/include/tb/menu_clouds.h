#ifndef TB_MENU_CLOUDS_H
#define TB_MENU_CLOUDS_H

#include <array>
#include <cstdint>

namespace tb
{
struct MenuCloud
{
    int x_fixed = 0;
    int y_fixed = 0;
    int vx = 0;
    int vy = 0;
    int type = 0;
};

class MenuCloudField
{
public:
    static constexpr int cloud_count = 8;

    MenuCloudField();

    void reset();
    [[nodiscard]] bool update(int delta_ms);
    [[nodiscard]] const MenuCloud& cloud(int index) const;

private:
    [[nodiscard]] int _next_bits(int bits);
    [[nodiscard]] int _next_random(int bound);
    void _spawn(MenuCloud& cloud, bool initial);

    static constexpr int _width_fixed = (256 * 240) / 22;
    static constexpr int _height_fixed = (256 * 160) / 22;
    static constexpr uint64_t _java_multiplier = 0x5DEECE66DULL;
    static constexpr uint64_t _java_addend = 0xBULL;
    static constexpr uint64_t _java_mask = (1ULL << 48) - 1;

    std::array<MenuCloud, cloud_count> _clouds{};
    int _update_accumulator_ms = 0;
    uint64_t _random_state = 0;
};
}

#endif
