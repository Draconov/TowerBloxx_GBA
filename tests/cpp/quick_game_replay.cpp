#include <iostream>

#include "tb/quick_game.h"

namespace
{
tb::InputFrame fresh_a()
{
    return tb::InputFrame{tb::key_mask(tb::Key::A), tb::key_mask(tb::Key::A)};
}

void emit(const tb::QuickGame& game, int tick)
{
    const tb::QuickGameSnapshot snapshot = game.snapshot();
    std::cout << tick << ',' << int(snapshot.status) << ',' << int(snapshot.block_state) << ','
              << snapshot.floor_count << ',' << snapshot.chances_left << ','
              << snapshot.camera_y << ',' << snapshot.camera_target_y << ',' << snapshot.current_x << ','
              << snapshot.current_y << ',' << snapshot.rope_length << ',' << snapshot.swing_phase_ms << ','
              << snapshot.swing_period_ms << ',' << snapshot.swing_amplitude_x << ',' << snapshot.swing_amplitude_y
              << ',' << snapshot.drop_velocity_x << ',' << snapshot.drop_velocity_y << ','
              << int(snapshot.last_accuracy) << ',' << snapshot.population << ',' << snapshot.combo_count << ','
              << snapshot.combo_bonus_pending << ',' << snapshot.combo_meter_ms << ',' << snapshot.longest_combo;
    for(int index = 0; index < game.floor_count(); ++index)
    {
        const tb::QuickFloor& floor = game.floor(index);
        std::cout << ';' << floor.x << ':' << floor.y << ':' << floor.offset;
    }
    std::cout << '\n';
}

void tick(tb::QuickGame& game, int& tick_index, const tb::InputFrame& input = {})
{
    game.update(25, input);
    ++tick_index;
    emit(game, tick_index);
}

void advance_until_ready(tb::QuickGame& game, int& tick_index)
{
    for(int guard = 0; guard < 300; ++guard)
    {
        const tb::QuickGameSnapshot snapshot = game.snapshot();
        if(snapshot.status != tb::QuickGameStatus::Playing ||
           (snapshot.block_state == tb::QuickBlockState::Attached && snapshot.camera_y == snapshot.camera_target_y))
        {
            return;
        }
        tick(game, tick_index);
    }
}
}

int main()
{
    constexpr int release_waits[] = {8, 44, 28, 29, 89, 89, 26, 29, 88, 25};
    tb::QuickGame game;
    int tick_index = 0;
    emit(game, tick_index);

    for(int goal = 1; goal <= 10; ++goal)
    {
        advance_until_ready(game, tick_index);
        for(int wait = 0; wait < release_waits[goal - 1]; ++wait)
        {
            tick(game, tick_index);
        }
        tick(game, tick_index, fresh_a());

        for(int guard = 0; guard < 180 && game.floor_count() < goal && game.snapshot().status == tb::QuickGameStatus::Playing;
            ++guard)
        {
            tick(game, tick_index);
        }

        if(game.floor_count() != goal)
        {
            std::cerr << "failed to land floor " << goal << '\n';
            return 2;
        }
    }

    constexpr int miss_waits[] = {0, 0, 3};
    for(int miss = 0; miss < 3; ++miss)
    {
        advance_until_ready(game, tick_index);
        const int chances_before = game.snapshot().chances_left;
        for(int wait = 0; wait < miss_waits[miss]; ++wait)
        {
            tick(game, tick_index);
        }
        tick(game, tick_index, fresh_a());
        for(int guard = 0; guard < 400 && game.snapshot().chances_left == chances_before; ++guard)
        {
            tick(game, tick_index);
        }
        if(game.snapshot().chances_left != chances_before - 1)
        {
            std::cerr << "failed to miss attempt " << (miss + 1) << '\n';
            return 3;
        }
    }

    for(int index = 0; index < 80; ++index)
    {
        tick(game, tick_index);
    }

    const tb::QuickGameSnapshot final_snapshot = game.snapshot();
    const tb::QuickGameResult final_result = game.result();
    std::cout << "RESULTS " << final_snapshot.floor_count << ' ' << final_snapshot.chances_left << ' '
              << int(final_snapshot.status) << ' ' << final_result.population << ' ' << final_result.longest_combo << '\n';
    return final_snapshot.status == tb::QuickGameStatus::Results ? 0 : 4;
}
