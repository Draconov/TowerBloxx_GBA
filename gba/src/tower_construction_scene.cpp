#include "tb/tower_construction_scene.h"

#include "tb/scene_backdrop.h"

#include "bn_bg_palettes.h"
#include "bn_color.h"
#include "bn_math.h"
#include "bn_string.h"
#include "bn_regular_bg_items_construction_bg.h"

#include "generated/tower_localization.h"
#include "generated/tower_mesh_assets.h"
#include "generated/tower_ui_assets.h"

namespace tb
{
namespace
{
constexpr int max_visible_floors = 5;
constexpr int platform_mesh_id = 9;
constexpr int crane_hook_mesh_id = 8;
constexpr int fixed_units_per_floor = 256;
constexpr int pixels_per_floor = 22;
constexpr int world_screen_baseline_y = 0;

constexpr const generated::UiCompositeAsset* construction_target_badge_frames[] = {
    &generated::construction_target_badge_f0, &generated::construction_target_badge_f1,
    &generated::construction_target_badge_f2, &generated::construction_target_badge_f3,
    &generated::construction_target_badge_f4,
};
constexpr const generated::UiCompositeAsset* construction_state_indicator_frames[] = {
    &generated::hud_state_indicator_f0, &generated::hud_state_indicator_f1,
    &generated::hud_state_indicator_f2, &generated::hud_state_indicator_f3,
    &generated::hud_state_indicator_f4, &generated::hud_state_indicator_f5,
    &generated::hud_state_indicator_f6, &generated::hud_state_indicator_f7,
    &generated::hud_state_indicator_f8, &generated::hud_state_indicator_f9,
};
constexpr const generated::UiCompositeAsset* construction_white_digit_frames[] = {
    &generated::hud_white_digit_f0, &generated::hud_white_digit_f1, &generated::hud_white_digit_f2,
    &generated::hud_white_digit_f3, &generated::hud_white_digit_f4, &generated::hud_white_digit_f5,
    &generated::hud_white_digit_f6, &generated::hud_white_digit_f7, &generated::hud_white_digit_f8,
    &generated::hud_white_digit_f9,
};

void show_ui_composite(const generated::UiCompositeAsset& asset, int x, int y,
                       bn::ivector<bn::sprite_ptr>& output, int z_order = -100)
{
    for(int index = 0; index < asset.part_count; ++index)
    {
        const generated::UiSpritePartAsset& part = asset.parts[index];
        bn::sprite_ptr sprite = part.item->create_sprite(x + part.x, y + part.y);
        sprite.set_z_order(z_order);
        output.push_back(sprite);
    }
}

void draw_source_number(int value, int min_digits, int right_x, int top_y,
                        const generated::UiCompositeAsset* const* digits,
                        bn::ivector<bn::sprite_ptr>& output)
{
    if(value < 0)
    {
        value = 0;
    }
    int rendered = 0;
    int left = right_x - 4;
    do
    {
        const int digit = value % 10;
        value /= 10;
        show_ui_composite(*digits[digit], left + 2 - 120, top_y + 3 - 80, output);
        left -= 4;
        ++rendered;
    }
    while(value > 0 || rendered < min_digits);
}

const generated::MeshAsset& mesh_by_id(int mesh_id)
{
    for(int index = 0; index < generated::mesh_count; ++index)
    {
        if(generated::meshes[index].mesh_id == mesh_id)
        {
            return generated::meshes[index];
        }
    }
    return generated::meshes[0];
}

void create_mesh_sprites(const generated::MeshAsset& mesh, bn::ivector<bn::sprite_ptr>& output)
{
    output.clear();
    for(int index = 0; index < mesh.part_count; ++index)
    {
        output.push_back(mesh.parts[index].item->create_sprite(0, 0));
    }
}

void position_mesh_sprites(const generated::MeshAsset& mesh, int x, int y, bn::ivector<bn::sprite_ptr>& sprites)
{
    for(int index = 0; index < mesh.part_count; ++index)
    {
        sprites[index].set_position(x + mesh.parts[index].x, y + mesh.parts[index].y);
    }
}

void position_rotated_mesh_part(
        const generated::MeshPartAsset& part, int x, int y, int angle_degrees,
        bn::sprite_affine_mat_ptr& affine_mat, bn::sprite_ptr& sprite)
{
    const bn::fixed safe_angle = bn::safe_degrees_angle(angle_degrees);
    affine_mat.set_rotation_angle(safe_angle);
    const bn::pair<bn::fixed, bn::fixed> sin_and_cos = bn::degrees_lut_sin_and_cos_safe(safe_angle);
    const bn::fixed rotated_x = part.x * sin_and_cos.second - part.y * sin_and_cos.first;
    const bn::fixed rotated_y = part.x * sin_and_cos.first + part.y * sin_and_cos.second;
    sprite.set_affine_mat(affine_mat);
    sprite.set_position(bn::fixed(x) + rotated_x, bn::fixed(y) + rotated_y);
}

}

TowerConstructionScene::TowerConstructionScene() :
    _current_affine_mat(bn::sprite_affine_mat_ptr::create()),
    _crane_affine_mat(bn::sprite_affine_mat_ptr::create()),
    _text_generator(generated::tower_font)
{
    _text_generator.set_center_alignment();
    _text_generator.set_z_order(-100);
}

void TowerConstructionScene::start(const BuildCityConstructionRequest& request, int language)
{
    set_gameplay_backdrop();
    _request = request;
    _language = language >= 0 && language < generated::locale_count ? language : 0;
    _construction.start(request.building_type, request.target_height, request.trophy_eligible);
    _frame_phase = 0;
    _rendered_floor_count = -1;
    _rendered_current_mesh_id = -1;
    _last_hud_floor_count = -1;
    _last_hud_chances = -1;
    _last_hud_population = -1;
    _last_hud_roof_phase = false;
    _last_hud_roof_result = 0;
    _last_hud_status = TowerConstructionStatus::Results;
    _active = true;
    _background = bn::regular_bg_items::construction_bg.create_bg(0, 0);
    _background->set_priority(3);
    _floor_affine_mats.clear();
    _floor_sprites.clear();
    _current_sprites.clear();
    _hud_sprites.clear();
    _ensure_crane_sprites();
    _rebuild_floor_sprites();
    const TowerConstructionSnapshot snapshot = _construction.snapshot();
    _rebuild_current_sprites(snapshot);
    _update_world_positions(snapshot);
    _rebuild_hud(snapshot);
}

TowerConstructionSceneUpdateResult TowerConstructionScene::update(const InputFrame& input)
{
    set_gameplay_backdrop();
    TowerConstructionSceneUpdateResult result;
    if(! _active)
    {
        result.exit = true;
        return result;
    }

    const TowerConstructionSnapshot before = _construction.snapshot();
    if(before.status == TowerConstructionStatus::Playing && input.pressed(Key::B))
    {
        _stop();
        result.exit = true;
        return result;
    }

    static constexpr int frame_deltas[] = {16, 17, 17};
    const int delta_ms = frame_deltas[_frame_phase];
    _frame_phase = (_frame_phase + 1) % 3;
    _construction.update(delta_ms, input);
    const TowerConstructionSnapshot snapshot = _construction.snapshot();

    if(snapshot.status == TowerConstructionStatus::Results)
    {
        const TowerConstructionResult construction_result = _construction.result();
        result.exit = true;
        result.completed = construction_result.ready;
        result.building_type = construction_result.building_type;
        result.population = construction_result.population;
        result.roof = construction_result.roof;
        _stop();
        return result;
    }

    if(snapshot.floor_count != _rendered_floor_count)
    {
        _rebuild_floor_sprites();
    }
    _rebuild_current_sprites(snapshot);
    _update_world_positions(snapshot);

    if(snapshot.floor_count != _last_hud_floor_count || snapshot.chances_left != _last_hud_chances ||
       snapshot.population != _last_hud_population || snapshot.roof_phase != _last_hud_roof_phase ||
       snapshot.roof_result != _last_hud_roof_result || snapshot.status != _last_hud_status)
    {
        _rebuild_hud(snapshot);
    }
    return result;
}

bool TowerConstructionScene::active() const
{
    return _active;
}

void TowerConstructionScene::_stop()
{
    _active = false;
    _background.reset();
    _floor_affine_mats.clear();
    _floor_sprites.clear();
    _current_sprites.clear();
    _platform_sprites.clear();
    _crane_hook_sprites.clear();
    _hud_sprites.clear();
}

void TowerConstructionScene::_rebuild_floor_sprites()
{
    _floor_affine_mats.clear();
    _floor_sprites.clear();
    _rendered_floor_count = _construction.floor_count();
    _visible_floor_start = _rendered_floor_count > max_visible_floors ? _rendered_floor_count - max_visible_floors : 0;

    const TowerConstructionSnapshot snapshot = _construction.snapshot();
    for(int floor_index = _visible_floor_start; floor_index < _rendered_floor_count; ++floor_index)
    {
        const TowerConstructionFloor& floor = _construction.floor(floor_index);
        const int mesh_id = floor.roof ? (snapshot.roof_result == 2 ? _trophy_roof_mesh_id() : _normal_roof_mesh_id()) :
                                         _normal_floor_mesh_id();
        const generated::MeshAsset& mesh = mesh_by_id(mesh_id);
        bn::sprite_affine_mat_ptr affine_mat = bn::sprite_affine_mat_ptr::create();
        _floor_affine_mats.push_back(affine_mat);
        for(int part_index = 0; part_index < mesh.part_count; ++part_index)
        {
            bn::sprite_ptr sprite = mesh.parts[part_index].item->create_sprite(0, 0);
            sprite.set_affine_mat(affine_mat);
            _floor_sprites.push_back(sprite);
        }
    }
}

void TowerConstructionScene::_rebuild_current_sprites(const TowerConstructionSnapshot& snapshot)
{
    const int mesh_id = snapshot.roof_phase ?
            (snapshot.trophy_eligible ? _trophy_roof_mesh_id() : _normal_roof_mesh_id()) : _normal_floor_mesh_id();
    if(mesh_id != _rendered_current_mesh_id)
    {
        create_mesh_sprites(mesh_by_id(mesh_id), _current_sprites);
        for(bn::sprite_ptr& sprite : _current_sprites)
        {
            sprite.set_affine_mat(_current_affine_mat);
        }
        _rendered_current_mesh_id = mesh_id;
    }
}

void TowerConstructionScene::_ensure_crane_sprites()
{
    if(_platform_sprites.empty())
    {
        create_mesh_sprites(mesh_by_id(platform_mesh_id), _platform_sprites);
    }
    if(_crane_hook_sprites.empty())
    {
        create_mesh_sprites(mesh_by_id(crane_hook_mesh_id), _crane_hook_sprites);
    }
}

void TowerConstructionScene::_update_world_positions(const TowerConstructionSnapshot& snapshot)
{
    int sprite_index = 0;
    int affine_index = 0;
    for(int floor_index = _visible_floor_start; floor_index < _rendered_floor_count; ++floor_index)
    {
        const TowerConstructionFloor& floor = _construction.floor(floor_index);
        const TowerConstructionRenderPose& pose = _construction.floor_render_pose(floor_index);
        const int mesh_id = floor.roof ? (snapshot.roof_result == 2 ? _trophy_roof_mesh_id() : _normal_roof_mesh_id()) :
                                         _normal_floor_mesh_id();
        const generated::MeshAsset& mesh = mesh_by_id(mesh_id);
        const int x = _screen_x(floor.x + pose.x_delta);
        const int y = _screen_y(floor.y + pose.y_delta, snapshot.presentation_camera_y);
        bn::sprite_affine_mat_ptr& affine_mat = _floor_affine_mats[affine_index];
        for(int part_index = 0; part_index < mesh.part_count; ++part_index)
        {
            position_rotated_mesh_part(mesh.parts[part_index], x, y, pose.z_angle_degrees, affine_mat,
                                       _floor_sprites[sprite_index]);
            ++sprite_index;
        }
        ++affine_index;
    }

    const bool current_visible = snapshot.status == TowerConstructionStatus::Playing &&
            (snapshot.block_state == TowerConstructionBlockState::Raising ||
             snapshot.block_state == TowerConstructionBlockState::Attached ||
             snapshot.block_state == TowerConstructionBlockState::Falling ||
             snapshot.block_state == TowerConstructionBlockState::Slipping);
    for(bn::sprite_ptr& sprite : _current_sprites)
    {
        sprite.set_visible(current_visible);
    }
    if(current_visible)
    {
        const generated::MeshAsset& mesh = mesh_by_id(_rendered_current_mesh_id);
        const int x = _screen_x(snapshot.current_x);
        const int y = _screen_y(snapshot.current_y, snapshot.presentation_camera_y);
        for(int part_index = 0; part_index < mesh.part_count; ++part_index)
        {
            position_rotated_mesh_part(mesh.parts[part_index], x, y, snapshot.current_z_angle_degrees,
                                       _current_affine_mat, _current_sprites[part_index]);
        }
    }

    const bool platform_visible = snapshot.floor_count <= 5;
    for(bn::sprite_ptr& sprite : _platform_sprites) { sprite.set_visible(platform_visible); }
    if(platform_visible)
    {
        position_mesh_sprites(mesh_by_id(platform_mesh_id), _screen_x(0),
                              _screen_y(0, snapshot.presentation_camera_y), _platform_sprites);
    }

    const bool crane_visible = snapshot.status == TowerConstructionStatus::Playing;
    for(bn::sprite_ptr& sprite : _crane_hook_sprites) { sprite.set_visible(crane_visible); }
    if(crane_visible)
    {
        const generated::MeshAsset& crane_mesh = mesh_by_id(crane_hook_mesh_id);
        // The original keeps crane/hook motion independent after release.
        // current_x/current_y become the ballistic block pose, while the crane
        // continues to swing from its own p/q coordinates.
        const int crane_x = _screen_x(snapshot.crane_x);
        const int crane_y = _screen_y(snapshot.crane_y, snapshot.presentation_camera_y);
        for(int part_index = 0; part_index < crane_mesh.part_count; ++part_index)
        {
            // M3G rotates in a Y-up world; sprite coordinates are Y-down.
            position_rotated_mesh_part(
                    crane_mesh.parts[part_index], crane_x, crane_y, -snapshot.crane_angle_degrees,
                    _crane_affine_mat, _crane_hook_sprites[part_index]);
        }
    }
}

void TowerConstructionScene::_rebuild_hud(const TowerConstructionSnapshot& snapshot)
{
    _hud_sprites.clear();
    _last_hud_floor_count = snapshot.floor_count;
    _last_hud_chances = snapshot.chances_left;
    _last_hud_population = snapshot.population;
    _last_hud_roof_phase = snapshot.roof_phase;
    _last_hud_roof_result = snapshot.roof_result;
    _last_hud_status = snapshot.status;

    int building_index = snapshot.building_type - 1;
    if(building_index < 0)
    {
        building_index = 0;
    }
    else if(building_index > 3)
    {
        building_index = 3;
    }

    // Common House.i(Graphics) chance/life strip. The original clips its
    // four-row draw loop to three visible 6x6 cells at x=25. Active cells use
    // the building color pair; exhausted cells use resource-18 frame 8.
    const int active_frame = building_index * 2;
    for(int slot = 0; slot < 3; ++slot)
    {
        const int required_chances = 3 - slot;
        const int frame = snapshot.chances_left >= required_chances ? active_frame : 8;
        const int top_y = 132 + slot * 6;
        show_ui_composite(*construction_state_indicator_frames[frame], -92, top_y + 3 - 80, _hud_sprites);
    }

    // B==3 construction branch: resource 13 selects frame L-1 and is placed
    // at x=au-2, y=c-av-12-2*J. J is the target floor count (10/20/30/40).
    const int badge_top_y = 160 - 10 - 12 - 2 * snapshot.target_height;
    show_ui_composite(*construction_target_badge_frames[building_index], -105,
                      badge_top_y + 6 - 80, _hud_sprites);

    // The population marker/digits are part of the common House HUD and only
    // appear after at least one floor has landed.
    if(snapshot.floor_count > 0)
    {
        show_ui_composite(generated::hud_population_icon, 82, 63, _hud_sprites);
        draw_source_number(snapshot.population, 5, 228, 141, construction_white_digit_frames, _hud_sprites);
    }

    if(snapshot.status == TowerConstructionStatus::Terminal)
    {
        if(snapshot.chances_left == 0)
        {
            _text_generator.generate(0, 0, generated::localized_strings[_language][56], _hud_sprites);
        }
        else if(snapshot.roof_result == 2)
        {
            _text_generator.generate(0, 0, generated::localized_strings[_language][57], _hud_sprites);
        }
    }
}

int TowerConstructionScene::_normal_floor_mesh_id() const
{
    return 9 + _request.building_type;
}

int TowerConstructionScene::_normal_roof_mesh_id() const
{
    return 29 + _request.building_type; // 30 + (type - 1)
}

int TowerConstructionScene::_trophy_roof_mesh_id() const
{
    return 39 + _request.building_type; // 40 + (type - 1)
}

int TowerConstructionScene::_screen_x(int world_x) const
{
    return (world_x * pixels_per_floor) / fixed_units_per_floor;
}

int TowerConstructionScene::_screen_y(int world_y, int camera_y) const
{
    return world_screen_baseline_y - ((world_y - camera_y) * pixels_per_floor) / fixed_units_per_floor;
}
}
