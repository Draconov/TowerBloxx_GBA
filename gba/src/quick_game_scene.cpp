#include "tb/quick_game_scene.h"

#include "tb/scene_backdrop.h"

#include "bn_bg_palettes.h"
#include "bn_color.h"
#include "bn_math.h"
#include "bn_string.h"
#include "bn_string_view.h"

#include "generated/tower_localization.h"
#include "generated/tower_mesh_assets.h"

namespace tb
{
namespace
{
constexpr int max_visible_floors = 5;
constexpr int floor_mesh_id = 10;
constexpr int crane_top_mesh_id = 9;
constexpr int crane_hook_mesh_id = 8;
constexpr int fixed_units_per_floor = 256;
constexpr int pixels_per_floor = 22;
constexpr int world_screen_baseline_y = 50;
constexpr int combo_meter_segments = 8;

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
    _floor_affine_mats.clear();
    _floor_sprites.clear();
    _current_sprites.clear();
    _crane_top_sprites.clear();
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
    if(_crane_top_sprites.empty())
    {
        create_mesh_sprites(mesh_by_id(crane_top_mesh_id), _crane_top_sprites);
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

    const bool crane_visible = snapshot.status == QuickGameStatus::Playing;
    for(bn::sprite_ptr& sprite : _crane_top_sprites)
    {
        sprite.set_visible(crane_visible);
    }
    for(bn::sprite_ptr& sprite : _crane_hook_sprites)
    {
        sprite.set_visible(crane_visible);
    }
    if(crane_visible)
    {
        position_mesh_sprites(mesh_by_id(crane_top_mesh_id), 0, -68, _crane_top_sprites);
        position_mesh_sprites(
                mesh_by_id(crane_hook_mesh_id), _screen_x(snapshot.current_x),
                _screen_y(snapshot.current_y, snapshot.presentation_camera_y) - 34, _crane_hook_sprites);
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

    _text_generator.generate(0, -70, generated::localized_strings[_language][91], _hud_sprites);

    if(snapshot.status == QuickGameStatus::Results)
    {
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

    bn::string<32> floors_text(generated::localized_strings[_language][80]);
    floors_text.append(": ");
    floors_text.append(bn::to_string<6>(snapshot.floor_count));
    _text_generator.generate(-72, 68, floors_text, _hud_sprites);

    bn::string<32> population_text(generated::localized_strings[_language][82]);
    population_text.append(": ");
    population_text.append(bn::to_string<10>(snapshot.population));
    _text_generator.generate(0, 52, population_text, _hud_sprites);

    bn::string<8> chances_text;
    for(int index = 0; index < 3; ++index)
    {
        chances_text.append(index < snapshot.chances_left ? '*' : '.');
    }
    _text_generator.generate(82, 68, chances_text, _hud_sprites);

    if(snapshot.combo_meter_ms > 0)
    {
        bn::string<32> combo_text("x");
        combo_text.append(bn::to_string<6>(snapshot.combo_count));
        combo_text.append(" [");
        const int filled = combo_bucket(snapshot);
        for(int index = 0; index < combo_meter_segments; ++index)
        {
            combo_text.append(index < filled ? '#' : '-');
        }
        combo_text.append(']');
        _text_generator.generate(0, -52, combo_text, _hud_sprites);
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
