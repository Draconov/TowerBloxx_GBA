#include "tb/tower_construction_scene.h"

#include "tb/scene_backdrop.h"
#include "tb/crane_presentation.h"

#include "bn_bg_palettes.h"
#include "bn_color.h"
#include "bn_math.h"
#include "bn_sprite_double_size_mode.h"
#include "bn_string.h"
#include "bn_string_view.h"
#include "bn_regular_bg_items_construction_bg.h"
#include "bn_sprite_items_crane_special_cable_segment.h"

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
constexpr int special_crane_mesh_id = 7;
constexpr int fixed_units_per_floor = 256;
constexpr int pixels_per_floor = 22;
constexpr int world_screen_baseline_y = 0;

constexpr int current_block_z_order = -20;
constexpr int crane_mesh_z_order = -10;
constexpr int special_cable_z_order = -5;
constexpr int modal_backdrop_z_order = -90;

constexpr const generated::UiCompositeAsset* gameplay_worker_blue_frames[] = {
    &generated::menu_worker_blue_f0, &generated::menu_worker_blue_f1,
    &generated::menu_worker_blue_f2, &generated::menu_worker_blue_f3,
    &generated::menu_worker_blue_f4, &generated::menu_worker_blue_f5,
    &generated::menu_worker_blue_f6, &generated::menu_worker_blue_f7,
};
constexpr const generated::UiCompositeAsset* gameplay_worker_red_frames[] = {
    &generated::menu_worker_red_f0, &generated::menu_worker_red_f1,
    &generated::menu_worker_red_f2, &generated::menu_worker_red_f3,
    &generated::menu_worker_red_f4, &generated::menu_worker_red_f5,
    &generated::menu_worker_red_f6, &generated::menu_worker_red_f7,
};

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
constexpr const generated::UiCompositeAsset* construction_meter_fill_frames[] = {
    &generated::construction_meter_fill_1, &generated::construction_meter_fill_2,
    &generated::construction_meter_fill_3, &generated::construction_meter_fill_4,
};
constexpr const generated::UiCompositeAsset* construction_meter_rails_frames[] = {
    &generated::construction_meter_rails_10, &generated::construction_meter_rails_20,
    &generated::construction_meter_rails_30, &generated::construction_meter_rails_40,
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

bn::string<128> format_construction_modal_line(bn::string_view text)
{
    bn::string<128> result;
    for(int index = 0; index < text.size(); ++index)
    {
        if(text[index] == '%' && index + 1 < text.size() && text[index + 1] == 'U')
        {
            result.append("3");
            ++index;
        }
        else
        {
            result.append(text[index]);
        }
    }
    return result;
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

void create_crane_hook_frame_sprites(
        const generated::CraneHookFrameAsset& frame, bn::ivector<bn::sprite_ptr>& output)
{
    output.clear();
    for(int index = 0; index < frame.part_count; ++index)
    {
        output.push_back(frame.parts[index].item->create_sprite(0, 0));
    }
}

void position_crane_hook_frame_sprites(
        const generated::CraneHookFrameAsset& frame, int x, int y,
        bn::ivector<bn::sprite_ptr>& sprites)
{
    for(int index = 0; index < frame.part_count; ++index)
    {
        sprites[index].set_position(x + frame.parts[index].x, y + frame.parts[index].y);
    }
}

void position_rotated_mesh_part(
        const generated::MeshPartAsset& part, int x, int y, int angle_degrees, int y_angle_degrees,
        bn::sprite_affine_mat_ptr& affine_mat, bn::sprite_ptr& sprite)
{
    const bn::fixed safe_angle = bn::safe_degrees_angle(angle_degrees);
    const bn::fixed safe_y_angle = bn::safe_degrees_angle(y_angle_degrees);
    bn::fixed y_scale = bn::degrees_lut_sin_and_cos_safe(safe_y_angle).second;
    if(y_scale < 0)
    {
        y_scale = -y_scale;
    }
    affine_mat.set_rotation_angle(safe_angle);
    affine_mat.set_horizontal_scale(y_scale);
    const bn::pair<bn::fixed, bn::fixed> sin_and_cos = bn::degrees_lut_sin_and_cos_safe(safe_angle);
    const bn::fixed perspective_x = part.x * y_scale;
    const bn::fixed rotated_x = perspective_x * sin_and_cos.second - part.y * sin_and_cos.first;
    const bn::fixed rotated_y = perspective_x * sin_and_cos.first + part.y * sin_and_cos.second;
    sprite.set_affine_mat(affine_mat);
    sprite.set_position(bn::fixed(x) + rotated_x, bn::fixed(y) + rotated_y);
}

}

TowerConstructionScene::TowerConstructionScene() :
    _current_affine_mat(bn::sprite_affine_mat_ptr::create()),
    _text_generator(generated::tower_font)
{
    _text_generator.set_center_alignment();
    _text_generator.set_z_order(-100);
}

void TowerConstructionScene::start(
        const BuildCityConstructionRequest& request, int language, const SaveData& save)
{
    set_gameplay_backdrop();
    _request = request;
    _language = language >= 0 && language < generated::locale_count ? language : 0;
    _construction.start(request.building_type, request.target_height, request.trophy_eligible);
    _gameplay_workers.reset();
    _frame_phase = 0;
    _rendered_floor_count = -1;
    _rendered_current_mesh_id = -1;
    _rendered_tumble_stage = 0;
    _rendered_crane_mesh_id = -1;
    _rendered_crane_rotation_step = 999;
    _last_hud_floor_count = -1;
    _last_hud_chances = -1;
    _last_hud_population = -1;
    _last_hud_roof_phase = false;
    _last_hud_roof_result = 0;
    _last_hud_status = TowerConstructionStatus::Results;
    _modal_localization_index = -1;
    if(! construction_instructions_seen(save))
    {
        _modal_localization_index = 55;
    }
    _pending_result = {};
    _pending_result_valid = false;
    _active = true;
    _background = bn::regular_bg_items::construction_bg.create_bg(0, 0);
    _background->set_priority(3);
    _floor_affine_mats.clear();
    _floor_sprites.clear();
    _current_sprites.clear();
    _worker_sprites.clear();
    _hud_sprites.clear();
    _rebuild_floor_sprites();
    const TowerConstructionSnapshot snapshot = _construction.snapshot();
    _ensure_crane_sprites(snapshot);
    _rebuild_current_sprites(snapshot);
    _update_world_positions(snapshot);
    _rebuild_hud(snapshot);
}

TowerConstructionSceneUpdateResult TowerConstructionScene::update(const InputFrame& input, SaveData& save)
{
    set_gameplay_backdrop();
    TowerConstructionSceneUpdateResult result;
    if(! _active)
    {
        result.exit = true;
        return result;
    }

    if(_modal_localization_index >= 0)
    {
        if(input.pressed(Key::A) || input.pressed(Key::Start))
        {
            if(_modal_localization_index == 55)
            {
                result.save_dirty = mark_construction_instructions_seen(save);
                _modal_localization_index = -1;
                _rebuild_hud(_construction.snapshot());
            }
            else if(_pending_result_valid)
            {
                result.exit = true;
                result.completed = _pending_result.ready;
                result.building_type = _pending_result.building_type;
                result.population = _pending_result.population;
                result.roof = _pending_result.roof;
                _pending_result_valid = false;
                _modal_localization_index = -1;
                _stop();
            }
        }
        return result;
    }

    const TowerConstructionSnapshot before = _construction.snapshot();
    if(before.status == TowerConstructionStatus::Playing && input.pressed(Key::B))
    {
        result.suspend_requested = true;
        return result;
    }

    static constexpr int frame_deltas[] = {16, 17, 17};
    const int delta_ms = frame_deltas[_frame_phase];
    _frame_phase = (_frame_phase + 1) % 3;
    const bool workers_advanced = _gameplay_workers.begin_frame(delta_ms);
    _construction.update(delta_ms, input);
    const TowerConstructionSnapshot snapshot = _construction.snapshot();
    const bool floor_added = snapshot.floor_count > before.floor_count;
    const GameplayWorkerWorld worker_world = _worker_world(snapshot);
    if(floor_added)
    {
        const TowerConstructionFloor& landed = _construction.floor(snapshot.floor_count - 1);
        const int absolute_offset = landed.offset < 0 ? -landed.offset : landed.offset;
        if(before.floor_count > 0)
        {
            _gameplay_workers.scatter_floor(before.floor_count, absolute_offset, worker_world);
        }
        // House.d() returns immediately during the special roof phase.
        if(! landed.roof)
        {
            _gameplay_workers.spawn_for_landing(absolute_offset, worker_world);
        }
    }
    if(workers_advanced)
    {
        _gameplay_workers.finish_frame(worker_world);
    }

    if(snapshot.status == TowerConstructionStatus::Results)
    {
        const TowerConstructionResult construction_result = _construction.result();
        if(construction_result.ready && (construction_result.roof == 0 || construction_result.roof == 2))
        {
            _pending_result = construction_result;
            _pending_result_valid = true;
            if(construction_result.roof == 0)
            {
                _modal_localization_index = 56;
            }
            else
            {
                _modal_localization_index = 57;
            }
            _rebuild_hud(snapshot);
            return result;
        }

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
    _ensure_crane_sprites(snapshot);
    _update_world_positions(snapshot);
    if(workers_advanced || floor_added)
    {
        _rebuild_worker_sprites(snapshot);
    }

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

void TowerConstructionScene::suspend_presentation()
{
    _background.reset();
    _floor_affine_mats.clear();
    _floor_sprites.clear();
    _current_sprites.clear();
    _platform_sprites.clear();
    _crane_hook_sprites.clear();
    _special_cable_sprites.clear();
    _worker_sprites.clear();
    _hud_sprites.clear();
    _rendered_current_mesh_id = -1;
    _rendered_tumble_stage = 0;
    _rendered_crane_mesh_id = -1;
    _rendered_crane_rotation_step = 999;
}

void TowerConstructionScene::resume_presentation()
{
    if(! _active)
    {
        return;
    }
    set_gameplay_backdrop();
    _background = bn::regular_bg_items::construction_bg.create_bg(0, 0);
    _background->set_priority(3);
    _rendered_floor_count = -1;
    _rendered_current_mesh_id = -1;
    _rendered_tumble_stage = 0;
    _rendered_crane_mesh_id = -1;
    _rendered_crane_rotation_step = 999;
    _last_hud_floor_count = -1;
    _last_hud_chances = -1;
    _last_hud_population = -1;
    _last_hud_roof_phase = false;
    _last_hud_roof_result = 0;
    _last_hud_status = TowerConstructionStatus::Results;
    _rebuild_floor_sprites();
    const TowerConstructionSnapshot snapshot = _construction.snapshot();
    _ensure_crane_sprites(snapshot);
    _rebuild_current_sprites(snapshot);
    _update_world_positions(snapshot);
    _rebuild_worker_sprites(snapshot);
    _rebuild_hud(snapshot);
}

void TowerConstructionScene::discard()
{
    _active = false;
    suspend_presentation();
}

void TowerConstructionScene::_stop()
{
    discard();
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
                                         (floor_index == 0 ? initial_base_mesh_id(_request.building_type) : _normal_floor_mesh_id());
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
    const bool current_visible = snapshot.status == TowerConstructionStatus::Playing &&
            (snapshot.block_state == TowerConstructionBlockState::Raising || snapshot.block_state == TowerConstructionBlockState::Attached ||
             snapshot.block_state == TowerConstructionBlockState::Falling || snapshot.block_state == TowerConstructionBlockState::Slipping);
    if(! current_visible)
    {
        _current_sprites.clear();
        _rendered_current_mesh_id = -1;
        _rendered_tumble_stage = 0;
        return;
    }

    const int mesh_id = snapshot.roof_phase ?
            (snapshot.trophy_eligible ? _trophy_roof_mesh_id() : _normal_roof_mesh_id()) :
            (snapshot.floor_count == 0 ? _initial_base_mesh_id() : _normal_floor_mesh_id());
    int tumble_stage = generated::tumble_stage_for_y_angle(snapshot.current_y_angle_degrees);
    if(! generated::tumble_pose_available(mesh_id))
    {
        tumble_stage = 0;
    }
    const bool z_negative = snapshot.current_z_angle_degrees < 0;
    const bool y_negative = snapshot.current_y_angle_degrees < 0;
    const bool presentation_changed = mesh_id != _rendered_current_mesh_id ||
            tumble_stage != _rendered_tumble_stage ||
            (tumble_stage > 0 && (z_negative != _rendered_tumble_z_negative ||
                                  y_negative != _rendered_tumble_y_negative));
    if(presentation_changed)
    {
        _current_sprites.clear();
        if(tumble_stage > 0)
        {
            const generated::TumblePoseAsset& pose = generated::tumble_pose_for(
                    mesh_id, tumble_stage, z_negative, y_negative);
            for(int part_index = 0; part_index < pose.part_count; ++part_index)
            {
                bn::sprite_ptr sprite = pose.parts[part_index].item->create_sprite(0, 0);
                sprite.set_z_order(current_block_z_order);
                _current_sprites.push_back(sprite);
            }
        }
        else
        {
            create_mesh_sprites(mesh_by_id(mesh_id), _current_sprites);
            for(bn::sprite_ptr& sprite : _current_sprites)
            {
                sprite.set_affine_mat(_current_affine_mat);
                sprite.set_z_order(current_block_z_order);
            }
        }
        _rendered_current_mesh_id = mesh_id;
        _rendered_tumble_stage = tumble_stage;
        _rendered_tumble_z_negative = z_negative;
        _rendered_tumble_y_negative = y_negative;
    }
}

void TowerConstructionScene::_ensure_crane_sprites(const TowerConstructionSnapshot& snapshot)
{
    if(_platform_sprites.empty())
    {
        create_mesh_sprites(mesh_by_id(platform_mesh_id), _platform_sprites);
    }

    const CranePresentationMode mode = crane_presentation_mode(
            snapshot.status == TowerConstructionStatus::Playing,
            snapshot.floor_count,
            snapshot.roof_phase,
            snapshot.block_state == TowerConstructionBlockState::Falling,
            snapshot.block_state == TowerConstructionBlockState::Missed);
    if(mode == CranePresentationMode::Hidden)
    {
        return;
    }

    if(mode == CranePresentationMode::Special)
    {
        if(_crane_hook_sprites.empty() || _rendered_crane_mesh_id != special_crane_mesh_id)
        {
            create_mesh_sprites(mesh_by_id(special_crane_mesh_id), _crane_hook_sprites);
            for(bn::sprite_ptr& sprite : _crane_hook_sprites)
            {
                sprite.set_z_order(crane_mesh_z_order);
            }
            _rendered_crane_mesh_id = special_crane_mesh_id;
            _rendered_crane_rotation_step = 999;
        }
        return;
    }

    const int rotation_step = snapshot.crane_x >> 4;
    const generated::CraneHookFrameAsset& frame = generated::crane_hook_frame_for_step(rotation_step);
    if(_crane_hook_sprites.empty() || _rendered_crane_mesh_id != crane_hook_mesh_id ||
       _rendered_crane_rotation_step != frame.rotation_step)
    {
        create_crane_hook_frame_sprites(frame, _crane_hook_sprites);
        for(bn::sprite_ptr& sprite : _crane_hook_sprites)
        {
            sprite.set_z_order(crane_mesh_z_order);
        }
        _rendered_crane_mesh_id = crane_hook_mesh_id;
        _rendered_crane_rotation_step = frame.rotation_step;
    }
}

void TowerConstructionScene::_rebuild_special_cable(const TowerConstructionSnapshot& snapshot, CranePresentationMode mode)
{
    if(mode != CranePresentationMode::Special)
    {
        _special_cable_sprites.clear();
        return;
    }

    // House.e(Graphics) starts this Java2D line at the screen centre in X.
    // Its source camera anchor is 1920 fixed units above the active camera:
    // -(22 * 1920 >> 8) = -165 in Butano's screen-centred coordinates.
    constexpr int start_x = 0;
    constexpr int start_y = -165;
    constexpr int endpoint_overlap = 3;
    const int end_x = _screen_x(snapshot.crane_x);
    const int end_y = _screen_y(snapshot.crane_y + 528, snapshot.presentation_camera_y);
    const int dx = end_x - start_x;
    const int dy = end_y - start_y;
    const int cable_length = bn::sqrt(dx * dx + dy * dy);
    if(cable_length <= 0)
    {
        _special_cable_sprites.clear();
        return;
    }

    if(_special_cable_sprites.empty())
    {
        bn::sprite_ptr sprite = bn::sprite_items::crane_special_cable_segment.create_sprite(0, 0);
        sprite.set_z_order(special_cable_z_order);
        sprite.set_double_size_mode(bn::sprite_double_size_mode::ENABLED);
        _special_cable_sprites.push_back(sprite);
    }

    // Java2D drawLine() includes both endpoints. Give the affine replacement
    // a three-pixel overlap at each end so fixed-point sampling cannot open a
    // one-frame gap where the long cable meets the V-shaped sling.
    const int cable_draw_length = cable_length + endpoint_overlap * 2;
    bn::sprite_ptr& sprite = _special_cable_sprites[0];
    sprite.set_position(bn::fixed(start_x + end_x) / 2, bn::fixed(start_y + end_y) / 2);
    sprite.set_vertical_scale(bn::fixed(cable_draw_length) / 64);
    sprite.set_rotation_angle_safe(bn::degrees_atan2(dx, dy));
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
                                         (floor_index == 0 ? initial_base_mesh_id(_request.building_type) : _normal_floor_mesh_id());
        const generated::MeshAsset& mesh = mesh_by_id(mesh_id);
        const int x = _screen_x(floor.x + pose.x_delta);
        const int y = _screen_y(floor.y + pose.y_delta, snapshot.presentation_camera_y);
        bn::sprite_affine_mat_ptr& affine_mat = _floor_affine_mats[affine_index];
        for(int part_index = 0; part_index < mesh.part_count; ++part_index)
        {
            position_rotated_mesh_part(mesh.parts[part_index], x, y, pose.z_angle_degrees, 0, affine_mat,
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
        const int x = _screen_x(snapshot.current_x);
        const int y = _screen_y(snapshot.current_y, snapshot.presentation_camera_y);
        if(_rendered_tumble_stage > 0)
        {
            const generated::TumblePoseAsset& pose = generated::tumble_pose_for(
                    _rendered_current_mesh_id, _rendered_tumble_stage,
                    _rendered_tumble_z_negative, _rendered_tumble_y_negative);
            for(int part_index = 0; part_index < pose.part_count; ++part_index)
            {
                _current_sprites[part_index].set_position(
                        x + pose.parts[part_index].x, y + pose.parts[part_index].y);
            }
        }
        else
        {
            const generated::MeshAsset& mesh = mesh_by_id(_rendered_current_mesh_id);
            for(int part_index = 0; part_index < mesh.part_count; ++part_index)
            {
                position_rotated_mesh_part(mesh.parts[part_index], x, y, snapshot.current_z_angle_degrees, 0,
                                           _current_affine_mat, _current_sprites[part_index]);
            }
        }
    }

    const bool platform_visible = snapshot.floor_count <= 5;
    for(bn::sprite_ptr& sprite : _platform_sprites) { sprite.set_visible(platform_visible); }
    if(platform_visible)
    {
        position_mesh_sprites(mesh_by_id(platform_mesh_id), _screen_x(0),
                              _screen_y(0, snapshot.presentation_camera_y), _platform_sprites);
    }

    const CranePresentationMode crane_mode = crane_presentation_mode(
            snapshot.status == TowerConstructionStatus::Playing,
            snapshot.floor_count,
            snapshot.roof_phase,
            snapshot.block_state == TowerConstructionBlockState::Falling,
            snapshot.block_state == TowerConstructionBlockState::Missed);
    const bool crane_visible = crane_mode != CranePresentationMode::Hidden;
    for(bn::sprite_ptr& sprite : _crane_hook_sprites) { sprite.set_visible(crane_visible); }
    if(crane_visible)
    {
        const int crane_x = _screen_x(snapshot.crane_x);
        const int crane_y = _screen_y(snapshot.crane_y, snapshot.presentation_camera_y);
        if(crane_mode == CranePresentationMode::Special)
        {
            position_mesh_sprites(
                    mesh_by_id(special_crane_mesh_id), crane_x, crane_y, _crane_hook_sprites);
        }
        else
        {
            const generated::CraneHookFrameAsset& frame =
                    generated::crane_hook_frame_for_step(snapshot.crane_x >> 4);
            position_crane_hook_frame_sprites(frame, crane_x, crane_y, _crane_hook_sprites);
        }
    }
    _rebuild_special_cable(snapshot, crane_mode);
}

GameplayWorkerWorld TowerConstructionScene::_worker_world(const TowerConstructionSnapshot& snapshot) const
{
    GameplayWorkerWorld world;
    world.camera_x = 0;
    world.camera_y = snapshot.camera_y;
    world.floor_count = snapshot.floor_count;
    const int slot_count = snapshot.floor_count < GameplayWorkerWorld::max_floor_slots ?
            snapshot.floor_count : GameplayWorkerWorld::max_floor_slots;
    world.floor_slot_count = slot_count;
    world.first_floor_number = snapshot.floor_count - slot_count + 1;
    for(int slot = 0; slot < slot_count; ++slot)
    {
        const int floor_index = world.first_floor_number + slot - 1;
        const TowerConstructionFloor& floor = _construction.floor(floor_index);
        world.floor_x[slot] = floor.x;
        world.floor_y[slot] = floor.y;
    }
    if(snapshot.floor_count > 0)
    {
        world.tower_x = _construction.floor(snapshot.floor_count - 1).x;
    }
    return world;
}

void TowerConstructionScene::_rebuild_worker_sprites(const TowerConstructionSnapshot& snapshot)
{
    _worker_sprites.clear();
    for(int index = 0; index < GameplayWorkerField::worker_count; ++index)
    {
        const GameplayWorker& worker = _gameplay_workers.worker(index);
        if(worker.state == 0)
        {
            continue;
        }
        const int frame = GameplayWorkerField::source_frame(worker);
        if(frame < 0 || frame >= 8)
        {
            continue;
        }
        const generated::UiCompositeAsset& asset = worker.variant == 1 ?
                *gameplay_worker_blue_frames[frame] : *gameplay_worker_red_frames[frame];
        show_ui_composite(
                asset, _screen_x(worker.x_fixed),
                _screen_y(worker.y_fixed, snapshot.presentation_camera_y), _worker_sprites, 10);
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

    // House.i(Graphics), B==3: the vertical target-height meter is anchored
    // at x=11 and grows upward two pixels per target floor.  The bottom cap
    // occupies y=148..149, while the target-1 normal floor slots occupy
    // y=146 upward in 2px rows.
    const int meter_left = 11;
    const int meter_top = 150 - 2 * snapshot.target_height;
    const int meter_slot_count = snapshot.target_height - 1;
    for(int slot = 0; slot < meter_slot_count; ++slot)
    {
        const int row_top = 146 - 2 * slot;
        const generated::UiCompositeAsset& row = slot < snapshot.floor_count ?
                *construction_meter_fill_frames[building_index] : generated::construction_meter_empty;
        show_ui_composite(row, meter_left + 4 - 120, row_top + 1 - 80, _hud_sprites);
    }
    show_ui_composite(generated::construction_meter_base, meter_left + 4 - 120, 149 - 80, _hud_sprites);
    show_ui_composite(
            *construction_meter_rails_frames[building_index],
            meter_left + 4 - 120, meter_top + snapshot.target_height - 80, _hud_sprites);

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

    if(_modal_localization_index >= 0)
    {
        _show_modal(_modal_localization_index);
    }
}

void TowerConstructionScene::_show_modal_backdrop(int line_count)
{
    (void) line_count;
    show_ui_composite(generated::dialog_window, 0, 0, _hud_sprites, modal_backdrop_z_order);
}

void TowerConstructionScene::_show_modal(int localization_index)
{
    const int modal_index = localization_index - generated::city_modal_min_string_index;
    if(modal_index < 0 || modal_index >= generated::city_modal_string_count)
    {
        return;
    }

    const int line_count = generated::city_modal_line_counts[_language][modal_index];
    _show_modal_backdrop(line_count);
    int y = -((line_count - 1) * 10) / 2;
    for(int line = 0; line < line_count; ++line)
    {
        const bn::string<128> formatted = format_construction_modal_line(
                generated::city_modal_lines[_language][modal_index][line]);
        _text_generator.generate(0, y, formatted, _hud_sprites);
        y += 10;
    }
    show_ui_composite(generated::city_continue_arrow, 112, 72, _hud_sprites);
}

int TowerConstructionScene::_normal_floor_mesh_id() const
{
    return normal_floor_mesh_id(_request.building_type);
}

int TowerConstructionScene::_initial_base_mesh_id() const
{
    return initial_base_mesh_id(_request.building_type);
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
