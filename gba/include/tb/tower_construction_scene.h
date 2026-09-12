#ifndef TB_TOWER_CONSTRUCTION_SCENE_H
#define TB_TOWER_CONSTRUCTION_SCENE_H

#include <cstdint>

#include "bn_sprite_affine_mat_ptr.h"
#include "bn_sprite_ptr.h"
#include "bn_sprite_text_generator.h"
#include "bn_vector.h"

#include "generated/tower_font.h"
#include "tb/build_city.h"
#include "tb/tower_construction.h"

namespace tb
{
struct TowerConstructionSceneUpdateResult
{
    bool exit = false;
    bool completed = false;
    uint8_t building_type = 0;
    int population = 0;
    uint8_t roof = 0;
};

class TowerConstructionScene
{
public:
    TowerConstructionScene();

    void start(const BuildCityConstructionRequest& request, int language);
    [[nodiscard]] TowerConstructionSceneUpdateResult update(const InputFrame& input);
    [[nodiscard]] bool active() const;

private:
    void _stop();
    void _rebuild_floor_sprites();
    void _rebuild_current_sprites(const TowerConstructionSnapshot& snapshot);
    void _ensure_crane_sprites();
    void _update_world_positions(const TowerConstructionSnapshot& snapshot);
    void _rebuild_hud(const TowerConstructionSnapshot& snapshot);
    [[nodiscard]] int _normal_floor_mesh_id() const;
    [[nodiscard]] int _normal_roof_mesh_id() const;
    [[nodiscard]] int _trophy_roof_mesh_id() const;
    [[nodiscard]] int _screen_x(int world_x) const;
    [[nodiscard]] int _screen_y(int world_y, int camera_y) const;

    TowerConstruction _construction;
    bn::sprite_affine_mat_ptr _current_affine_mat;
    bn::sprite_text_generator _text_generator;
    bn::vector<bn::sprite_affine_mat_ptr, 5> _floor_affine_mats;
    bn::vector<bn::sprite_ptr, 24> _floor_sprites;
    bn::vector<bn::sprite_ptr, 4> _current_sprites;
    bn::vector<bn::sprite_ptr, 3> _crane_top_sprites;
    bn::vector<bn::sprite_ptr, 2> _crane_hook_sprites;
    bn::vector<bn::sprite_ptr, 96> _hud_sprites;
    BuildCityConstructionRequest _request{};
    int _language = 0;
    int _frame_phase = 0;
    int _rendered_floor_count = -1;
    int _visible_floor_start = 0;
    int _rendered_current_mesh_id = -1;
    int _last_hud_floor_count = -1;
    int _last_hud_chances = -1;
    int _last_hud_population = -1;
    bool _last_hud_roof_phase = false;
    uint8_t _last_hud_roof_result = 0;
    TowerConstructionStatus _last_hud_status = TowerConstructionStatus::Results;
    bool _active = false;
};
}

#endif
