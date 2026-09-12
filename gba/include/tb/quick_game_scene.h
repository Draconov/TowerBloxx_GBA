#ifndef TB_QUICK_GAME_SCENE_H
#define TB_QUICK_GAME_SCENE_H

#include "bn_sprite_ptr.h"
#include "bn_sprite_text_generator.h"
#include "bn_vector.h"

#include "generated/tower_font.h"
#include "tb/app_state.h"
#include "tb/quick_game.h"
#include "tb/save_data.h"

namespace tb
{
struct QuickGameSceneUpdateResult
{
    bool exit = false;
    bool save_dirty = false;
};

class QuickGameScene
{
public:
    QuickGameScene();

    void start(int language);
    [[nodiscard]] QuickGameSceneUpdateResult update(const InputFrame& input, SaveData& save);
    [[nodiscard]] bool active() const;

private:
    void _stop();
    void _rebuild_floor_sprites();
    void _ensure_current_sprites();
    void _ensure_crane_sprites();
    void _update_world_positions();
    void _rebuild_hud(const QuickGameSnapshot& snapshot);
    [[nodiscard]] int _screen_x(int world_x) const;
    [[nodiscard]] int _screen_y(int world_y, int camera_y) const;

    QuickGame _game;
    QuickRecordFlags _record_flags;
    bn::sprite_text_generator _text_generator;
    bn::vector<bn::sprite_ptr, 32> _floor_sprites;
    bn::vector<bn::sprite_ptr, 4> _current_sprites;
    bn::vector<bn::sprite_ptr, 3> _crane_top_sprites;
    bn::vector<bn::sprite_ptr, 2> _crane_hook_sprites;
    bn::vector<bn::sprite_ptr, 96> _hud_sprites;
    int _language = 0;
    int _frame_phase = 0;
    int _rendered_floor_count = -1;
    int _visible_floor_start = 0;
    int _last_hud_floor_count = -1;
    int _last_hud_chances = -1;
    int _last_hud_population = -1;
    int _last_hud_combo_count = -1;
    int _last_hud_combo_bucket = -1;
    QuickGameStatus _last_hud_status = QuickGameStatus::GameOver;
    bool _records_applied = false;
    bool _active = false;
};
}

#endif
