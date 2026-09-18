#ifndef TB_QUICK_GAME_SCENE_H
#define TB_QUICK_GAME_SCENE_H

#include "bn_sprite_affine_mat_ptr.h"
#include "bn_optional.h"
#include "bn_sprite_ptr.h"
#include "bn_sprite_text_generator.h"
#include "bn_vector.h"

#include "generated/tower_font.h"
#include "tb/app_state.h"
#include "tb/construction_backdrop.h"
#include "tb/crane_presentation.h"
#include "tb/gameplay_workers.h"
#include "tb/life_indicator_animation.h"
#include "tb/quick_game.h"
#include "tb/save_data.h"

namespace tb
{
struct QuickGameSceneUpdateResult
{
    bool exit = false;
    bool save_dirty = false;
    bool suspend_requested = false;
    bool score_ready = false;
    uint32_t final_population = 0;
};

class QuickGameScene
{
public:
    QuickGameScene();

    void start(int language);
    [[nodiscard]] QuickGameSceneUpdateResult update(const InputFrame& input, SaveData& save);
    [[nodiscard]] bool active() const;
    void suspend_presentation();
    void resume_presentation();
    void discard();

private:
    void _stop();
    void _rebuild_floor_sprites();
    void _rebuild_current_sprites(const QuickGameSnapshot& snapshot);
    void _ensure_crane_sprites(const QuickGameSnapshot& snapshot);
    void _rebuild_special_cable(const QuickGameSnapshot& snapshot, CranePresentationMode mode);
    void _update_world_positions();
    [[nodiscard]] GameplayWorkerWorld _worker_world(const QuickGameSnapshot& snapshot) const;
    void _rebuild_worker_sprites(const QuickGameSnapshot& snapshot);
    void _rebuild_hud(const QuickGameSnapshot& snapshot);
    void _update_combo_meter(const QuickGameSnapshot& snapshot);
    void _update_block_sparkle(const QuickGameSnapshot& snapshot);
    void _update_perfect_landing_effect(const QuickGameSnapshot& snapshot);
    [[nodiscard]] int _screen_x(int world_x) const;
    [[nodiscard]] int _screen_y(int world_y, int camera_y) const;

    QuickGame _game;
    GameplayWorkerField _gameplay_workers;
    LifeIndicatorAnimation _life_indicator_animation;
    ConstructionBackdrop _backdrop;
    QuickRecordFlags _record_flags;
    bn::sprite_affine_mat_ptr _current_affine_mat;
    bn::sprite_text_generator _text_generator;
    bn::vector<bn::sprite_affine_mat_ptr, 5> _floor_affine_mats;
    bn::vector<bn::sprite_ptr, 32> _floor_sprites;
    bn::vector<bn::sprite_ptr, 4> _current_sprites;
    bn::vector<bn::sprite_ptr, 4> _platform_sprites;
    bn::vector<bn::sprite_ptr, 2> _crane_hook_sprites;
    bn::vector<bn::sprite_ptr, 16> _special_cable_sprites;
    bn::vector<bn::sprite_ptr, 3> _special_boom_sprites;
    bn::vector<bn::sprite_ptr, 16> _worker_sprites;
    bn::vector<bn::sprite_ptr, 96> _hud_sprites;
    bn::vector<bn::sprite_ptr, 2> _combo_star_sprites;
    bn::vector<bn::sprite_ptr, 8> _block_sparkle_sprites;
    bn::vector<bn::sprite_ptr, perfect_landing_sprite_capacity> _perfect_star_sprites;
    bn::optional<bn::sprite_ptr> _combo_meter_fill_sprite;
    bn::optional<bn::sprite_ptr> _combo_meter_flash_sprite;
    bn::optional<bn::sprite_ptr> _perfect_seam_sprite;
    PerfectLandingSeamPhase _perfect_seam_phase = PerfectLandingSeamPhase::Hidden;
    int _perfect_landing_elapsed_ms = -1;
    int _perfect_landing_floor_index = -1;
    int _perfect_landing_seed = 0;
    int _language = 0;
    int _frame_phase = 0;
    int _rendered_floor_count = -1;
    int _visible_floor_start = 0;
    int _rendered_current_mesh_id = -1;
    int _rendered_tumble_stage = 0;
    bool _rendered_tumble_z_negative = false;
    bool _rendered_tumble_y_negative = false;
    int _rendered_crane_mesh_id = -1;
    int _rendered_crane_rotation_step = 999;
    int _last_hud_floor_count = -1;
    int _last_hud_chances = -1;
    int _last_hud_population = -1;
    int _last_hud_combo_count = -1;
    int _last_hud_combo_bucket = -1;
    int _last_hud_combo_bonus_bucket = -1;
    int _combo_star_frame = -1;
    QuickGameStatus _last_hud_status = QuickGameStatus::GameOver;
    bool _records_applied = false;
    bool _active = false;
    int _background_clock_ms = 0;
};
}

#endif
