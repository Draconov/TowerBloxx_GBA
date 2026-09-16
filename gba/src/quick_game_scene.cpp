#include "tb/quick_game_scene.h"

#include "tb/scene_backdrop.h"
#include "tb/crane_presentation.h"

#include "bn_bg_palettes.h"
#include "bn_color.h"
#include "bn_math.h"
#include "bn_sprite_double_size_mode.h"
#include "bn_string.h"
#include "bn_string_view.h"
#include "bn_sprite_items_crane_special_cable_segment.h"

#include "generated/tower_localization.h"
#include "generated/tower_mesh_assets.h"
#include "generated/legacy_high_altitude_assets.h"
#include "generated/tower_ui_assets.h"

namespace tb
{
namespace
{
constexpr int max_visible_floors = 5;
constexpr int quick_building_type = 4;
constexpr int floor_mesh_id = 13;
constexpr int platform_mesh_id = 9;
constexpr int crane_hook_mesh_id = 8;
constexpr int special_crane_mesh_id = 7;
constexpr int fixed_units_per_floor = 256;
constexpr int pixels_per_floor = 22;
constexpr int world_screen_baseline_y = 0;

constexpr int current_block_z_order = -20;
constexpr int crane_mesh_z_order = -10;
constexpr int special_cable_z_order = -5;
constexpr int gameplay_worker_z_order = -30;
constexpr int combo_meter_segments = 8;
constexpr int combo_meter_max_width = 120;
constexpr int combo_meter_fill_left = -60;
constexpr int combo_meter_fill_y = -67;
constexpr int combo_meter_frame_x = 0;
constexpr int combo_meter_frame_y = -67;
constexpr int combo_star_x = -60;
constexpr int combo_star_y = -67;
constexpr int combo_readout_x = 68;
constexpr int combo_readout_y = -65;
constexpr int construction_sky_band_step = 3072;

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

constexpr const generated::UiCompositeAsset* hud_white_digit_frames[] = {
    &generated::hud_white_digit_f0, &generated::hud_white_digit_f1, &generated::hud_white_digit_f2,
    &generated::hud_white_digit_f3, &generated::hud_white_digit_f4, &generated::hud_white_digit_f5,
    &generated::hud_white_digit_f6, &generated::hud_white_digit_f7, &generated::hud_white_digit_f8,
    &generated::hud_white_digit_f9,
};
constexpr const generated::UiCompositeAsset* hud_brown_digit_frames[] = {
    &generated::hud_brown_digit_f0, &generated::hud_brown_digit_f1, &generated::hud_brown_digit_f2,
    &generated::hud_brown_digit_f3, &generated::hud_brown_digit_f4, &generated::hud_brown_digit_f5,
    &generated::hud_brown_digit_f6, &generated::hud_brown_digit_f7, &generated::hud_brown_digit_f8,
    &generated::hud_brown_digit_f9, &generated::hud_brown_digit_f10, &generated::hud_brown_digit_f11,
};
constexpr const generated::UiCompositeAsset* hud_state_indicator_frames[] = {
    &generated::hud_state_indicator_f0, &generated::hud_state_indicator_f1,
    &generated::hud_state_indicator_f2, &generated::hud_state_indicator_f3,
    &generated::hud_state_indicator_f4, &generated::hud_state_indicator_f5,
    &generated::hud_state_indicator_f6, &generated::hud_state_indicator_f7,
    &generated::hud_state_indicator_f8, &generated::hud_state_indicator_f9,
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
        // House.a/b digit helpers clip a 5x7 source cell at x=(argX-4) and
        // then step four pixels left, deliberately overlapping by one pixel.
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

bn::string<64> format_result_line(bn::string_view template_text, int value)
{
    bn::string<64> result;
    for(int index = 0; index < template_text.size(); ++index)
    {
        if(template_text[index] == '%' && index + 1 < template_text.size() && template_text[index + 1] == 'U')
        {
            result.append(bn::to_string<12>(value));
            ++index;
        }
        else
        {
            result.append(template_text[index]);
        }
    }
    return result;
}

void append_record_marker(bn::string<64>& text, bool record, const char* marker)
{
    if(record)
    {
        text.append(" ");
        text.append(marker);
    }
}

int combo_bucket(const QuickGameSnapshot& snapshot)
{
    if(snapshot.combo_meter_ms <= 0)
    {
        return 0;
    }
    int bucket = (snapshot.combo_meter_ms * combo_meter_segments + 5999) / 6000;
    if(bucket < 1)
    {
        bucket = 1;
    }
    if(bucket > combo_meter_segments)
    {
        bucket = combo_meter_segments;
    }
    return bucket;
}
}

QuickGameScene::QuickGameScene() :
    _current_affine_mat(bn::sprite_affine_mat_ptr::create()),
    _text_generator(generated::tower_font)
{
    _text_generator.set_center_alignment();
    _text_generator.set_z_order(-100);
}

void QuickGameScene::start(int language)
{
    set_gameplay_backdrop();
    _language = language >= 0 && language < generated::locale_count ? language : 0;
    _game.reset();
    _gameplay_workers.reset();
    _life_indicator_animation.reset(3);
    _record_flags = {};
    _records_applied = false;
    _frame_phase = 0;
    _rendered_floor_count = -1;
    _rendered_current_mesh_id = -1;
    _rendered_tumble_stage = 0;
    _rendered_crane_mesh_id = -1;
    _rendered_crane_rotation_step = 999;
    _last_hud_floor_count = -1;
    _last_hud_chances = -1;
    _last_hud_population = -1;
    _last_hud_combo_count = -1;
    _last_hud_combo_bucket = -1;
    _last_hud_status = QuickGameStatus::GameOver;
    _active = true;
    _background_clock_ms = 0;

    _floor_affine_mats.clear();
    _floor_sprites.clear();
    _worker_sprites.clear();
    _hud_sprites.clear();
    _combo_meter_fill_sprite.reset();
    _combo_meter_flash_sprite.reset();
    _combo_star_sprites.clear();
    _combo_star_frame = -1;
    _block_sparkle_sprites.clear();
    _block_sparkle_frame = -1;
    _block_sparkle_sprites.clear();
    _block_sparkle_frame = -1;
    _rebuild_floor_sprites();
    const QuickGameSnapshot snapshot = _game.snapshot();
    _rebuild_current_sprites(snapshot);
    _ensure_crane_sprites(snapshot);
    _backdrop.start(snapshot.presentation_camera_y, _background_clock_ms);
    _update_world_positions();
    _update_block_sparkle(snapshot);
    _rebuild_hud(snapshot);
    _update_combo_meter(snapshot);
}

QuickGameSceneUpdateResult QuickGameScene::update(const InputFrame& input, SaveData& save)
{
    set_gameplay_backdrop();
    QuickGameSceneUpdateResult result;
    if(! _active)
    {
        result.exit = true;
        return result;
    }

    const QuickGameStatus before_status = _game.snapshot().status;
    if(before_status == QuickGameStatus::Playing && input.pressed(Key::B))
    {
        result.suspend_requested = true;
        return result;
    }
    if(before_status == QuickGameStatus::Results && input.pressed(Key::A))
    {
        const QuickGameResult final_result = _game.result();
        _stop();
        result.score_ready = true;
        result.final_population = uint32_t(final_result.population);
        return result;
    }
    if(before_status == QuickGameStatus::Results && input.pressed(Key::B))
    {
        _stop();
        result.exit = true;
        return result;
    }

    // 16,17,17 ms repeated is exactly 50 ms per three 60 Hz frames.
    static constexpr int frame_deltas[] = {16, 17, 17};
    const int delta_ms = frame_deltas[_frame_phase];
    _frame_phase = (_frame_phase + 1) % 3;
    const QuickGameSnapshot before = _game.snapshot();
    const bool workers_advanced = _gameplay_workers.begin_frame(delta_ms);
    _game.update(delta_ms, input);

    const QuickGameSnapshot snapshot = _game.snapshot();
    const bool life_indicator_changed = _life_indicator_animation.advance(delta_ms, snapshot.chances_left);
    const bool floor_added = snapshot.floor_count > before.floor_count;
    const GameplayWorkerWorld worker_world = _worker_world(snapshot);
    if(floor_added)
    {
        const QuickFloor& landed = _game.floor(snapshot.floor_count - 1);
        const int absolute_offset = landed.offset < 0 ? -landed.offset : landed.offset;
        if(before.floor_count > 0)
        {
            _gameplay_workers.scatter_floor(before.floor_count, absolute_offset, worker_world);
        }
        _gameplay_workers.spawn_for_landing(absolute_offset, worker_world);
    }
    if(workers_advanced)
    {
        _gameplay_workers.finish_frame(worker_world);
    }
    if(snapshot.status == QuickGameStatus::Results && ! _records_applied)
    {
        const QuickGameResult game_result = _game.result();
        _record_flags = apply_quick_result(save, game_result);
        _records_applied = true;
        result.save_dirty = _record_flags.any();
    }

    if(snapshot.floor_count != _rendered_floor_count)
    {
        _rebuild_floor_sprites();
    }
    _rebuild_current_sprites(snapshot);
    _ensure_crane_sprites(snapshot);
    _background_clock_ms += delta_ms;
    _backdrop.update(snapshot.presentation_camera_y, _background_clock_ms);
    _update_world_positions();
    _update_block_sparkle(snapshot);
    if(workers_advanced || floor_added)
    {
        _rebuild_worker_sprites(snapshot);
    }

    const int current_combo_bucket = combo_bucket(snapshot);
    if(life_indicator_changed || snapshot.floor_count != _last_hud_floor_count || snapshot.chances_left != _last_hud_chances ||
       snapshot.population != _last_hud_population || snapshot.combo_count != _last_hud_combo_count ||
       current_combo_bucket != _last_hud_combo_bucket || snapshot.status != _last_hud_status)
    {
        _rebuild_hud(snapshot);
    }
    _update_combo_meter(snapshot);

    return result;
}

bool QuickGameScene::active() const
{
    return _active;
}

void QuickGameScene::suspend_presentation()
{
    _backdrop.reset();
    _floor_affine_mats.clear();
    _floor_sprites.clear();
    _current_sprites.clear();
    _platform_sprites.clear();
    _crane_hook_sprites.clear();
    _special_cable_sprites.clear();
    _worker_sprites.clear();
    _hud_sprites.clear();
    _combo_meter_fill_sprite.reset();
    _combo_meter_flash_sprite.reset();
    _combo_star_sprites.clear();
    _combo_star_frame = -1;
}

void QuickGameScene::resume_presentation()
{
    if(! _active)
    {
        return;
    }
    set_gameplay_backdrop();
    _rendered_floor_count = -1;
    _rendered_current_mesh_id = -1;
    _rendered_tumble_stage = 0;
    _rendered_crane_mesh_id = -1;
    _rendered_crane_rotation_step = 999;
    _last_hud_floor_count = -1;
    _last_hud_chances = -1;
    _last_hud_population = -1;
    _last_hud_combo_count = -1;
    _last_hud_combo_bucket = -1;
    _last_hud_status = QuickGameStatus::GameOver;
    _rebuild_floor_sprites();
    const QuickGameSnapshot snapshot = _game.snapshot();
    _rebuild_current_sprites(snapshot);
    _ensure_crane_sprites(snapshot);
    _backdrop.start(snapshot.presentation_camera_y, _background_clock_ms);
    _update_world_positions();
    _update_block_sparkle(snapshot);
    _rebuild_worker_sprites(snapshot);
    _rebuild_hud(snapshot);
    _update_combo_meter(snapshot);
}

void QuickGameScene::discard()
{
    _active = false;
    suspend_presentation();
}

void QuickGameScene::_stop()
{
    discard();
}

void QuickGameScene::_rebuild_floor_sprites()
{
    _floor_affine_mats.clear();
    _floor_sprites.clear();
    _rendered_floor_count = _game.floor_count();
    _visible_floor_start = _rendered_floor_count > max_visible_floors ? _rendered_floor_count - max_visible_floors : 0;

    for(int floor_index = _visible_floor_start; floor_index < _rendered_floor_count; ++floor_index)
    {
        const int mesh_id = floor_index == 0 ? initial_base_mesh_id(quick_building_type) :
                                               floor_mesh_id;
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

void QuickGameScene::_rebuild_current_sprites(const QuickGameSnapshot& snapshot)
{
    const bool current_visible = snapshot.status == QuickGameStatus::Playing &&
            (snapshot.block_state == QuickBlockState::Raising || snapshot.block_state == QuickBlockState::Attached ||
             snapshot.block_state == QuickBlockState::Falling || snapshot.block_state == QuickBlockState::Slipping);
    if(! current_visible)
    {
        _current_sprites.clear();
        _rendered_current_mesh_id = -1;
        _rendered_tumble_stage = 0;
        return;
    }

    const int mesh_id = snapshot.floor_count == 0 ? initial_base_mesh_id(quick_building_type) :
                                                    floor_mesh_id;
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

void QuickGameScene::_ensure_crane_sprites(const QuickGameSnapshot& snapshot)
{
    if(_platform_sprites.empty())
    {
        create_mesh_sprites(mesh_by_id(platform_mesh_id), _platform_sprites);
    }

    const CranePresentationMode mode = crane_presentation_mode(
            snapshot.status == QuickGameStatus::Playing,
            snapshot.floor_count,
            false,
            snapshot.block_state == QuickBlockState::Falling,
            snapshot.block_state == QuickBlockState::Missed);
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

void QuickGameScene::_rebuild_special_cable(const QuickGameSnapshot& snapshot, CranePresentationMode mode)
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

void QuickGameScene::_update_world_positions()
{
    const QuickGameSnapshot snapshot = _game.snapshot();

    int sprite_index = 0;
    int affine_index = 0;
    for(int floor_index = _visible_floor_start; floor_index < _rendered_floor_count; ++floor_index)
    {
        const QuickFloor& floor = _game.floor(floor_index);
        const QuickFloorRenderPose& pose = _game.floor_render_pose(floor_index);
        const int mesh_id = floor_index == 0 ? initial_base_mesh_id(quick_building_type) :
                                               floor_mesh_id;
        const generated::MeshAsset& floor_mesh = mesh_by_id(mesh_id);
        const int x = _screen_x(floor.x + pose.x_delta);
        const int y = _screen_y(floor.y + pose.y_delta, snapshot.presentation_camera_y);
        bn::sprite_affine_mat_ptr& affine_mat = _floor_affine_mats[affine_index];
        for(int part_index = 0; part_index < floor_mesh.part_count; ++part_index)
        {
            position_rotated_mesh_part(
                    floor_mesh.parts[part_index], x, y, pose.z_angle_degrees, 0, affine_mat,
                    _floor_sprites[sprite_index]);
            ++sprite_index;
        }
        ++affine_index;
    }

    const bool current_visible = snapshot.status == QuickGameStatus::Playing &&
            (snapshot.block_state == QuickBlockState::Raising || snapshot.block_state == QuickBlockState::Attached ||
             snapshot.block_state == QuickBlockState::Falling || snapshot.block_state == QuickBlockState::Slipping);
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
            const generated::MeshAsset& floor_mesh = mesh_by_id(_rendered_current_mesh_id);
            for(int part_index = 0; part_index < floor_mesh.part_count; ++part_index)
            {
                position_rotated_mesh_part(
                        floor_mesh.parts[part_index], x, y, snapshot.current_z_angle_degrees,
                        0, _current_affine_mat, _current_sprites[part_index]);
            }
        }
    }

    const bool platform_visible = snapshot.floor_count <= 5;
    for(bn::sprite_ptr& sprite : _platform_sprites)
    {
        sprite.set_visible(platform_visible);
    }
    if(platform_visible)
    {
        position_mesh_sprites(
                mesh_by_id(platform_mesh_id), _screen_x(0),
                _screen_y(0, snapshot.presentation_camera_y), _platform_sprites);
    }

    const CranePresentationMode mode = crane_presentation_mode(
            snapshot.status == QuickGameStatus::Playing,
            snapshot.floor_count,
            false,
            snapshot.block_state == QuickBlockState::Falling,
            snapshot.block_state == QuickBlockState::Missed);
    const bool crane_visible = mode != CranePresentationMode::Hidden;
    for(bn::sprite_ptr& sprite : _crane_hook_sprites)
    {
        sprite.set_visible(crane_visible);
    }
    if(crane_visible)
    {
        const int crane_x = _screen_x(snapshot.crane_x);
        const int crane_y = _screen_y(snapshot.crane_y, snapshot.presentation_camera_y);
        if(mode == CranePresentationMode::Special)
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
    _rebuild_special_cable(snapshot, mode);
}

GameplayWorkerWorld QuickGameScene::_worker_world(const QuickGameSnapshot& snapshot) const
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
        const QuickFloor& floor = _game.floor(floor_index);
        world.floor_x[slot] = floor.x;
        world.floor_y[slot] = floor.y;
    }
    if(snapshot.floor_count > 0)
    {
        world.tower_x = _game.floor(snapshot.floor_count - 1).x;
    }
    return world;
}

void QuickGameScene::_rebuild_worker_sprites(const QuickGameSnapshot& snapshot)
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
                _screen_y(worker.y_fixed, snapshot.presentation_camera_y), _worker_sprites,
                gameplay_worker_z_order);
    }
}

void QuickGameScene::_update_combo_meter(const QuickGameSnapshot& snapshot)
{
    if(snapshot.status != QuickGameStatus::Playing || snapshot.combo_meter_ms <= 0)
    {
        _combo_meter_fill_sprite.reset();
        _combo_meter_flash_sprite.reset();
        _combo_star_sprites.clear();
        _combo_star_frame = -1;
        return;
    }

    int width = snapshot.combo_meter_ms * combo_meter_max_width / 6000;
    if(width < 1)
    {
        width = 1;
    }
    else if(width > combo_meter_max_width)
    {
        width = combo_meter_max_width;
    }

    const bool flash = snapshot.combo_meter_ms > 5850;
    bn::optional<bn::sprite_ptr>& active = flash ? _combo_meter_flash_sprite : _combo_meter_fill_sprite;
    bn::optional<bn::sprite_ptr>& inactive = flash ? _combo_meter_fill_sprite : _combo_meter_flash_sprite;
    inactive.reset();

    if(! active)
    {
        const generated::UiCompositeAsset& asset = flash ? generated::quick_combo_meter_flash :
                                                            generated::quick_combo_meter_fill;
        const generated::UiSpritePartAsset& part = asset.parts[0];
        bn::sprite_ptr sprite = part.item->create_sprite(0, 0);
        sprite.set_z_order(-100);
        sprite.set_double_size_mode(bn::sprite_double_size_mode::ENABLED);
        active = sprite;
    }

    bn::sprite_ptr& sprite = *active;
    const int part_y = flash ? generated::quick_combo_meter_flash.parts[0].y :
                               generated::quick_combo_meter_fill.parts[0].y;
    sprite.set_horizontal_scale(bn::fixed(width) / 64);
    sprite.set_position(bn::fixed(combo_meter_fill_left * 2 + width) / 2, combo_meter_fill_y + part_y);

    // Nokia v1.3.37 House.i(Graphics): resource 46 is a 4-frame 22x22
    // combo star and advances on the global game clock every 80 ms.
    if(snapshot.combo_count > 1)
    {
        const int frame = (_background_clock_ms / 80) % 4;
        if(frame != _combo_star_frame)
        {
            _combo_star_sprites.clear();
            show_ui_composite(*generated::legacy_combo_star_frames[frame],
                              combo_star_x, combo_star_y, _combo_star_sprites, -102);
            _combo_star_frame = frame;
        }
    }
    else
    {
        _combo_star_sprites.clear();
        _combo_star_frame = -1;
    }
}

void QuickGameScene::_update_block_sparkle(const QuickGameSnapshot& snapshot)
{
    // Nokia v1.3.37 House.i(Graphics): resource 47 surrounds the active block
    // only while state 1/2 is active (Attached/Falling), cycling 3 frames at
    // 100 ms.  The GBA compositor keeps the source 44x44 frame centred on the
    // current block world position.
    const bool visible = snapshot.status == QuickGameStatus::Playing &&
            (snapshot.block_state == QuickBlockState::Attached ||
             snapshot.block_state == QuickBlockState::Falling);
    if(! visible)
    {
        _block_sparkle_sprites.clear();
        _block_sparkle_frame = -1;
        return;
    }

    const int frame = (_background_clock_ms / 100) % 3;
    const generated::UiCompositeAsset& asset = *generated::legacy_block_sparkle_frames[frame];
    if(frame != _block_sparkle_frame)
    {
        _block_sparkle_sprites.clear();
        show_ui_composite(asset, 0, 0, _block_sparkle_sprites, -21);
        _block_sparkle_frame = frame;
    }

    const int center_x = _screen_x(snapshot.current_x);
    const int center_y = _screen_y(snapshot.current_y, snapshot.presentation_camera_y);
    for(int index = 0; index < asset.part_count; ++index)
    {
        const generated::UiSpritePartAsset& part = asset.parts[index];
        _block_sparkle_sprites[index].set_position(center_x + part.x, center_y + part.y);
    }
}

void QuickGameScene::_rebuild_hud(const QuickGameSnapshot& snapshot)
{
    _hud_sprites.clear();
    _last_hud_floor_count = snapshot.floor_count;
    _last_hud_chances = snapshot.chances_left;
    _last_hud_population = snapshot.population;
    _last_hud_combo_count = snapshot.combo_count;
    _last_hud_combo_bucket = combo_bucket(snapshot);
    _last_hud_status = snapshot.status;

    if(snapshot.status == QuickGameStatus::Results)
    {
        show_ui_composite(generated::dialog_window, 0, 0, _hud_sprites, -90);
        const QuickGameResult final_result = _game.result();
        bn::string<64> population = format_result_line(generated::localized_strings[_language][93], final_result.population);
        bn::string<64> height = format_result_line(generated::localized_strings[_language][94], final_result.height);
        bn::string<64> combo = format_result_line(generated::localized_strings[_language][95], final_result.longest_combo);
        const char* record_marker = generated::localized_strings[_language][96];
        append_record_marker(population, _record_flags.population, record_marker);
        append_record_marker(height, _record_flags.height, record_marker);
        append_record_marker(combo, _record_flags.combo, record_marker);
        _text_generator.generate(0, -24, population, _hud_sprites);
        _text_generator.generate(0, 0, height, _hud_sprites);
        _text_generator.generate(0, 24, combo, _hud_sprites);

        bn::string<32> back_text("A  ");
        back_text.append(generated::localized_strings[_language][7]);
        _text_generator.generate(0, 52, back_text, _hud_sprites);
        return;
    }

    // Exact lower-left Quick Game frame from House.i(Graphics): resource 19
    // at (au-4, c-av-28), with au=12,av=10 for the 240x160 GBA view.
    show_ui_composite(generated::quick_counter_frame, -106, 56, _hud_sprites);
    draw_source_number(snapshot.floor_count, 3, 20, 139, hud_white_digit_frames, _hud_sprites);

    // Resource 18 is a color-pair strip. Quick Game uses the orange pair
    // (L=4 => base frame 6); frame 8 is the exhausted gray state. The JAR
    // clips the four-row loop so only three 6x6 cells are visible.
    for(int slot = 0; slot < 3; ++slot)
    {
        const int frame = _life_indicator_animation.frame_for_slot(slot, snapshot.chances_left, 6);
        const int top_y = 132 + slot * 6;
        show_ui_composite(*hud_state_indicator_frames[frame], -92, top_y + 3 - 80, _hud_sprites);
    }

    // House.i clips the top 6x9 of resource 17 for the population marker and
    // draws a five-digit resource-14 number to its right once the tower exists.
    if(snapshot.floor_count > 0)
    {
        show_ui_composite(generated::hud_population_icon, 82, 63, _hud_sprites);
        draw_source_number(snapshot.population, 5, 228, 141, hud_white_digit_frames, _hud_sprites);
    }

    if(snapshot.combo_meter_ms > 0)
    {
        show_ui_composite(
                generated::quick_combo_meter_frame, combo_meter_frame_x, combo_meter_frame_y,
                _hud_sprites, -101);
    }

    // The source combo readout uses resource 15: cell 11 is the x marker,
    // followed by one or two brown digits.
    if(snapshot.combo_meter_ms > 0 && snapshot.combo_count > 1)
    {
        show_ui_composite(*hud_brown_digit_frames[11], combo_readout_x, combo_readout_y, _hud_sprites);
        const int digits = snapshot.combo_count > 9 ? 2 : 1;
        draw_source_number(snapshot.combo_count, digits, 191 + digits * 5, 12,
                           hud_brown_digit_frames, _hud_sprites);
    }

    if(snapshot.status == QuickGameStatus::GameOver)
    {
        _text_generator.generate(0, 0, generated::localized_strings[_language][114], _hud_sprites);
    }
}

int QuickGameScene::_screen_x(int world_x) const
{
    return (world_x * pixels_per_floor) / fixed_units_per_floor;
}

int QuickGameScene::_screen_y(int world_y, int camera_y) const
{
    return world_screen_baseline_y - ((world_y - camera_y) * pixels_per_floor) / fixed_units_per_floor;
}
}
