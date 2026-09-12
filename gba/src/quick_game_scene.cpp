#include "tb/quick_game_scene.h"

#include "tb/scene_backdrop.h"

#include "bn_bg_palettes.h"
#include "bn_color.h"
#include "bn_math.h"
#include "bn_string.h"
#include "bn_string_view.h"
#include "bn_regular_bg_items_construction_bg.h"

#include "generated/tower_localization.h"
#include "generated/tower_mesh_assets.h"
#include "generated/tower_ui_assets.h"

namespace tb
{
namespace
{
constexpr int max_visible_floors = 5;
constexpr int floor_mesh_id = 13;
constexpr int platform_mesh_id = 9;
constexpr int crane_hook_mesh_id = 8;
constexpr int fixed_units_per_floor = 256;
constexpr int pixels_per_floor = 22;
constexpr int world_screen_baseline_y = 0;
constexpr int combo_meter_segments = 8;

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
    _crane_affine_mat(bn::sprite_affine_mat_ptr::create()),
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
    _record_flags = {};
    _records_applied = false;
    _frame_phase = 0;
    _rendered_floor_count = -1;
    _last_hud_floor_count = -1;
    _last_hud_chances = -1;
    _last_hud_population = -1;
    _last_hud_combo_count = -1;
    _last_hud_combo_bucket = -1;
    _last_hud_status = QuickGameStatus::GameOver;
    _active = true;
    _background = bn::regular_bg_items::construction_bg.create_bg(0, 0);
    _background->set_priority(3);

    _floor_affine_mats.clear();
    _floor_sprites.clear();
    _hud_sprites.clear();
    _ensure_current_sprites();
    _ensure_crane_sprites();
    _rebuild_floor_sprites();
    const QuickGameSnapshot snapshot = _game.snapshot();
    _update_world_positions();
    _rebuild_hud(snapshot);
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
        _stop();
        result.exit = true;
        return result;
    }
    if(before_status == QuickGameStatus::Results && (input.pressed(Key::A) || input.pressed(Key::B)))
    {
        _stop();
        result.exit = true;
        return result;
    }

    // 16,17,17 ms repeated is exactly 50 ms per three 60 Hz frames.
    static constexpr int frame_deltas[] = {16, 17, 17};
    const int delta_ms = frame_deltas[_frame_phase];
    _frame_phase = (_frame_phase + 1) % 3;
    _game.update(delta_ms, input);

    const QuickGameSnapshot snapshot = _game.snapshot();
    if(snapshot.status == QuickGameStatus::Results && ! _records_applied)
    {
        _record_flags = apply_quick_result(save, _game.result());
        _records_applied = true;
        result.save_dirty = _record_flags.any();
    }

    if(snapshot.floor_count != _rendered_floor_count)
    {
        _rebuild_floor_sprites();
    }
    _update_world_positions();

    const int current_combo_bucket = combo_bucket(snapshot);
    if(snapshot.floor_count != _last_hud_floor_count || snapshot.chances_left != _last_hud_chances ||
       snapshot.population != _last_hud_population || snapshot.combo_count != _last_hud_combo_count ||
       current_combo_bucket != _last_hud_combo_bucket || snapshot.status != _last_hud_status)
    {
        _rebuild_hud(snapshot);
    }

    return result;
}

bool QuickGameScene::active() const
{
    return _active;
}

void QuickGameScene::_stop()
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

void QuickGameScene::_rebuild_floor_sprites()
{
    _floor_affine_mats.clear();
    _floor_sprites.clear();
    _rendered_floor_count = _game.floor_count();
    _visible_floor_start = _rendered_floor_count > max_visible_floors ? _rendered_floor_count - max_visible_floors : 0;

    const generated::MeshAsset& mesh = mesh_by_id(floor_mesh_id);
    for(int floor_index = _visible_floor_start; floor_index < _rendered_floor_count; ++floor_index)
    {
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

void QuickGameScene::_ensure_current_sprites()
{
    if(_current_sprites.empty())
    {
        create_mesh_sprites(mesh_by_id(floor_mesh_id), _current_sprites);
        for(bn::sprite_ptr& sprite : _current_sprites)
        {
            sprite.set_affine_mat(_current_affine_mat);
        }
    }
}

void QuickGameScene::_ensure_crane_sprites()
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

void QuickGameScene::_update_world_positions()
{
    const QuickGameSnapshot snapshot = _game.snapshot();
    const generated::MeshAsset& floor_mesh = mesh_by_id(floor_mesh_id);

    int sprite_index = 0;
    int affine_index = 0;
    for(int floor_index = _visible_floor_start; floor_index < _rendered_floor_count; ++floor_index)
    {
        const QuickFloor& floor = _game.floor(floor_index);
        const QuickFloorRenderPose& pose = _game.floor_render_pose(floor_index);
        const int x = _screen_x(floor.x + pose.x_delta);
        const int y = _screen_y(floor.y + pose.y_delta, snapshot.presentation_camera_y);
        bn::sprite_affine_mat_ptr& affine_mat = _floor_affine_mats[affine_index];
        for(int part_index = 0; part_index < floor_mesh.part_count; ++part_index)
        {
            position_rotated_mesh_part(
                    floor_mesh.parts[part_index], x, y, pose.z_angle_degrees, affine_mat,
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
        for(int part_index = 0; part_index < floor_mesh.part_count; ++part_index)
        {
            position_rotated_mesh_part(
                    floor_mesh.parts[part_index], x, y, snapshot.current_z_angle_degrees, _current_affine_mat,
                    _current_sprites[part_index]);
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

    const bool crane_visible = snapshot.status == QuickGameStatus::Playing;
    for(bn::sprite_ptr& sprite : _crane_hook_sprites)
    {
        sprite.set_visible(crane_visible);
    }
    if(crane_visible)
    {
        const generated::MeshAsset& crane_mesh = mesh_by_id(crane_hook_mesh_id);
        const int crane_x = _screen_x(snapshot.current_x);
        const int crane_y = _screen_y(snapshot.current_y, snapshot.presentation_camera_y);
        for(int part_index = 0; part_index < crane_mesh.part_count; ++part_index)
        {
            // M3G rotates in a Y-up world; sprite coordinates are Y-down.
            position_rotated_mesh_part(
                    crane_mesh.parts[part_index], crane_x, crane_y, -snapshot.crane_angle_degrees,
                    _crane_affine_mat, _crane_hook_sprites[part_index]);
        }
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
        _text_generator.generate(0, -70, generated::localized_strings[_language][91], _hud_sprites);
        const QuickGameResult final_result = _game.result();
        bn::string<64> population = format_result_line(generated::localized_strings[_language][93], final_result.population);
        bn::string<64> height = format_result_line(generated::localized_strings[_language][94], final_result.height);
        bn::string<64> combo = format_result_line(generated::localized_strings[_language][95], final_result.longest_combo);
        const char* record_marker = generated::localized_strings[_language][96];
        append_record_marker(population, _record_flags.population, record_marker);
        append_record_marker(height, _record_flags.height, record_marker);
        append_record_marker(combo, _record_flags.combo, record_marker);
        _text_generator.generate(0, -28, population, _hud_sprites);
        _text_generator.generate(0, -4, height, _hud_sprites);
        _text_generator.generate(0, 20, combo, _hud_sprites);

        bn::string<32> back_text("A/B  ");
        back_text.append(generated::localized_strings[_language][7]);
        _text_generator.generate(0, 54, back_text, _hud_sprites);
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
        const int required_chances = 3 - slot;
        const int frame = snapshot.chances_left >= required_chances ? 6 : 8;
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

    // The source combo readout uses resource 15: cell 11 is the x marker,
    // followed by one or two brown digits. The dynamic meter geometry itself
    // remains a dedicated follow-up instead of being replaced by ASCII bars.
    if(snapshot.combo_meter_ms > 0 && snapshot.combo_count > 1)
    {
        show_ui_composite(*hud_brown_digit_frames[11], 66, -67, _hud_sprites);
        const int digits = snapshot.combo_count > 9 ? 2 : 1;
        draw_source_number(snapshot.combo_count, digits, 190 + digits * 5, 10,
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
