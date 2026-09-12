#include "tb/tower_construction_scene.h"

#include "tb/scene_backdrop.h"

#include "bn_bg_palettes.h"
#include "bn_color.h"
#include "bn_math.h"
#include "bn_string.h"

#include "generated/tower_localization.h"
#include "generated/tower_mesh_assets.h"

namespace tb
{
namespace
{
constexpr int max_visible_floors = 5;
constexpr int crane_top_mesh_id = 9;
constexpr int crane_hook_mesh_id = 8;
constexpr int fixed_units_per_floor = 256;
constexpr int pixels_per_floor = 22;
constexpr int world_screen_baseline_y = 50;

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

bn::string<40> value_line(const char* label, int value)
{
    bn::string<40> result(label);
    result.append(": ");
    result.append(bn::to_string<10>(value));
    return result;
}
}

TowerConstructionScene::TowerConstructionScene() :
    _current_affine_mat(bn::sprite_affine_mat_ptr::create()),
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
    _floor_affine_mats.clear();
    _floor_sprites.clear();
    _current_sprites.clear();
    _crane_top_sprites.clear();
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
    if(_crane_top_sprites.empty())
    {
        create_mesh_sprites(mesh_by_id(crane_top_mesh_id), _crane_top_sprites);
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

    const bool crane_visible = snapshot.status == TowerConstructionStatus::Playing;
    for(bn::sprite_ptr& sprite : _crane_top_sprites) { sprite.set_visible(crane_visible); }
    for(bn::sprite_ptr& sprite : _crane_hook_sprites) { sprite.set_visible(crane_visible); }
    if(crane_visible)
    {
        position_mesh_sprites(mesh_by_id(crane_top_mesh_id), 0, -68, _crane_top_sprites);
        position_mesh_sprites(mesh_by_id(crane_hook_mesh_id), _screen_x(snapshot.current_x),
                              _screen_y(snapshot.current_y, snapshot.presentation_camera_y) - 34,
                              _crane_hook_sprites);
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

    _text_generator.generate(0, -70, generated::localized_strings[_language][83 + snapshot.building_type - 1], _hud_sprites);
    _text_generator.generate(-72, 68, value_line(generated::localized_strings[_language][80], snapshot.floor_count), _hud_sprites);
    _text_generator.generate(0, 52, value_line(generated::localized_strings[_language][82], snapshot.population), _hud_sprites);

    bn::string<24> target("/");
    target.append(bn::to_string<4>(snapshot.target_height));
    _text_generator.generate(-34, 68, target, _hud_sprites);

    bn::string<8> chances;
    for(int index = 0; index < 3; ++index) { chances.append(index < snapshot.chances_left ? '*' : '.'); }
    _text_generator.generate(82, 68, chances, _hud_sprites);

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
