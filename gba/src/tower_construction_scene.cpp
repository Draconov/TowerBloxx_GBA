#include "tb/tower_construction_scene.h"

#include "tb/scene_backdrop.h"
#include "tb/crane_presentation.h"

#include "bn_bg_palettes.h"
#include "bn_color.h"
#include "bn_math.h"
#include "bn_sprite_double_size_mode.h"
#include "bn_sprites.h"
#include "bn_string.h"
#include "bn_string_view.h"
#include "bn_sprite_items_crane_special_cable_segment.h"
#include "bn_sprite_items_crane_special_boom_p0.h"
#include "bn_sprite_items_crane_special_boom_p1.h"
#include "bn_sprite_items_crane_special_boom_p2.h"

#include "generated/legacy_high_altitude_assets.h"
#include "generated/tower_localization.h"
#include "generated/tower_mesh_assets.h"
#include "generated/christmas_tower_mesh_assets.h"
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
constexpr int special_boom_z_order = -4;
constexpr int gameplay_worker_z_order = -30;
constexpr int construction_sky_band_step = 3072;
constexpr int modal_backdrop_z_order = -90;
constexpr int modal_line_spacing = 12;
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

constexpr const generated::UiCompositeAsset* gameplay_worker_blue_flying_frames[] = {
    &generated::menu_worker_blue_f1, &generated::menu_worker_blue_f2,
    &generated::menu_worker_blue_f3, &generated::menu_worker_blue_f4,
    &generated::menu_worker_blue_f5,
};
constexpr const generated::UiCompositeAsset* gameplay_worker_red_flying_frames[] = {
    &generated::menu_worker_red_f1, &generated::menu_worker_red_f2,
    &generated::menu_worker_red_f3, &generated::menu_worker_red_f4,
    &generated::menu_worker_red_f5,
};
constexpr const generated::UiCompositeAsset* gameplay_worker_blue_crawling_frames[] = {
    &generated::menu_worker_blue_f6, &generated::menu_worker_blue_f7,
    &generated::menu_worker_blue_f8, &generated::menu_worker_blue_f9,
};
constexpr const generated::UiCompositeAsset* gameplay_worker_red_crawling_frames[] = {
    &generated::menu_worker_red_f6, &generated::menu_worker_red_f7,
    &generated::menu_worker_red_f8, &generated::menu_worker_red_f9,
};

const generated::UiCompositeAsset& gameplay_worker_asset(const GameplayWorker& worker)
{
    const bool blue = worker.variant == 1;
    switch(worker.state)
    {
    case 1:
    case 3:
    {
        int index = worker.frame - 1;
        if(index < 0)
        {
            index = 0;
        }
        else if(index > 4)
        {
            index = 4;
        }
        return blue ? *gameplay_worker_blue_flying_frames[index] :
                      *gameplay_worker_red_flying_frames[index];
    }

    case 2:
    case 5:
    {
        int base = worker.walk_direction < 0 ? 2 : 0;
        int step = worker.frame <= 6 ? 0 : 1;
        return blue ? *gameplay_worker_blue_crawling_frames[base + step] :
                      *gameplay_worker_red_crawling_frames[base + step];
    }

    case 4:
    default:
        return blue ? generated::menu_worker_blue_f0 : generated::menu_worker_red_f0;
    }
}

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
constexpr const generated::UiCompositeAsset* hud_brown_digit_frames[] = {
    &generated::hud_brown_digit_f0, &generated::hud_brown_digit_f1, &generated::hud_brown_digit_f2,
    &generated::hud_brown_digit_f3, &generated::hud_brown_digit_f4, &generated::hud_brown_digit_f5,
    &generated::hud_brown_digit_f6, &generated::hud_brown_digit_f7, &generated::hud_brown_digit_f8,
    &generated::hud_brown_digit_f9, &generated::hud_brown_digit_f10, &generated::hud_brown_digit_f11,
};

void show_ui_composite(const generated::UiCompositeAsset& asset, int x, int y,
                       bn::ivector<bn::sprite_ptr>& output, int z_order = -100)
{
    // Whole-composite, fallible allocation: never draw a partial frame or
    // consume reserved OAM intended for the moving tower / the main HUD.
    if(output.max_size() - output.size() < asset.part_count ||
       bn::sprites::available_items_count() < asset.part_count)
    {
        return;
    }
    const int first = output.size();
    for(int index = 0; index < asset.part_count; ++index)
    {
        const generated::UiSpritePartAsset& part = asset.parts[index];
        bn::optional<bn::sprite_ptr> sprite = part.item->create_sprite_optional(x + part.x, y + part.y);
        if(! sprite)
        {
            while(output.size() > first) { output.pop_back(); }
            return;
        }
        sprite->set_z_order(z_order);
        output.push_back(*sprite);
    }
}


int draw_source_number(int value, int min_digits, int right_x, int top_y,
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
    return left;
}

int combo_bucket(const TowerConstructionSnapshot& snapshot)
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

int combo_bonus_blink_bucket(const TowerConstructionSnapshot& snapshot)
{
    if(snapshot.combo_bonus_pending == 0 || snapshot.combo_meter_ms > 0 || snapshot.combo_meter_ms <= -2000)
    {
        return -1;
    }
    return (-snapshot.combo_meter_ms) / 100;
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

const generated::MeshAsset& mesh_by_id(VisualTheme visual_theme, int mesh_id)
{
    if(visual_theme == VisualTheme::Christmas)
    {
        if(const generated::MeshAsset* christmas_mesh = generated::christmas::mesh_by_id(mesh_id))
        {
            return *christmas_mesh;
        }
    }

    for(int index = 0; index < generated::mesh_count; ++index)
    {
        if(generated::meshes[index].mesh_id == mesh_id)
        {
            return generated::meshes[index];
        }
    }
    return generated::meshes[0];
}

bool tumble_pose_available(VisualTheme visual_theme, int mesh_id)
{
    if(visual_theme == VisualTheme::Christmas)
    {
        return generated::christmas::tumble_pose_available(mesh_id);
    }
    return generated::tumble_pose_available(mesh_id);
}

const generated::TumblePoseAsset& tumble_pose_for(
        VisualTheme visual_theme, int mesh_id, int stage, bool z_negative, bool y_negative)
{
    if(visual_theme == VisualTheme::Christmas && generated::christmas::tumble_pose_available(mesh_id))
    {
        return generated::christmas::tumble_pose_for(mesh_id, stage, z_negative, y_negative);
    }
    return generated::tumble_pose_for(mesh_id, stage, z_negative, y_negative);
}

const generated::CraneHookFrameAsset& crane_hook_frame_for_step(VisualTheme visual_theme, int step)
{
    if(visual_theme == VisualTheme::Christmas)
    {
        return generated::christmas::crane_hook_frame_for_step(step);
    }
    return generated::crane_hook_frame_for_step(step);
}

void create_mesh_sprites(const generated::MeshAsset& mesh, bn::ivector<bn::sprite_ptr>& output)
{
    output.clear();
    if(mesh.part_count > output.max_size() ||
       bn::sprites::available_items_count() < mesh.part_count)
    {
        return;
    }
    for(int index = 0; index < mesh.part_count; ++index)
    {
        bn::optional<bn::sprite_ptr> sprite = mesh.parts[index].item->create_sprite_optional(0, 0);
        if(! sprite)
        {
            output.clear();
            return; // Retry next frame instead of crashing due to OBJ VRAM/palettes.
        }
        output.push_back(*sprite);
    }
}

void position_mesh_sprites(const generated::MeshAsset& mesh, int x, int y, bn::ivector<bn::sprite_ptr>& sprites)
{
    if(sprites.size() != mesh.part_count) { return; }
    for(int index = 0; index < mesh.part_count; ++index)
    {
        sprites[index].set_position(x + mesh.parts[index].x, y + mesh.parts[index].y);
    }
}

void create_crane_hook_frame_sprites(
        const generated::CraneHookFrameAsset& frame, bn::ivector<bn::sprite_ptr>& output)
{
    output.clear();
    if(frame.part_count > output.max_size() ||
       bn::sprites::available_items_count() < frame.part_count)
    {
        return;
    }
    for(int index = 0; index < frame.part_count; ++index)
    {
        bn::optional<bn::sprite_ptr> sprite = frame.parts[index].item->create_sprite_optional(0, 0);
        if(! sprite)
        {
            output.clear();
            return;
        }
        output.push_back(*sprite);
    }
}

void position_crane_hook_frame_sprites(
        const generated::CraneHookFrameAsset& frame, int x, int y,
        bn::ivector<bn::sprite_ptr>& sprites)
{
    if(sprites.size() != frame.part_count) { return; }
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
    _visual_theme = visual_theme(save);
    _construction.start(request.building_type, request.target_height, request.trophy_eligible, request.stationary_crane);
    _gameplay_workers.reset();
    _life_indicator_animation.reset(3);
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
    _last_hud_combo_bonus_bucket = -1;
    _combo_star_frame = -1;
    _last_hud_roof_blink_bucket = -1;
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
    _background_clock_ms = 0;
    _floor_affine_mats.clear();
    _floor_sprites.clear();
    _current_sprites.clear();
    _worker_sprites.clear();
    _hud_sprites.clear();
    _combo_meter_fill_sprite.reset();
    _combo_meter_flash_sprite.reset();
    _perfect_seam_sprite.reset();
    _perfect_seam_phase = PerfectLandingSeamPhase::Hidden;
    _perfect_landing_elapsed_ms = -1;
    _perfect_landing_floor_index = -1;
    _perfect_landing_seed = 0;
    _perfect_star_sprites.clear();
    _combo_star_sprites.clear();
    _combo_star_frame = -1;
    _block_sparkle_sprites.clear();
    _rebuild_floor_sprites();
    const TowerConstructionSnapshot snapshot = _construction.snapshot();
    _ensure_crane_sprites(snapshot);
    _rebuild_current_sprites(snapshot);
    _update_world_positions(snapshot);
    _rebuild_hud(snapshot);
    _update_combo_meter(snapshot);
    _update_perfect_landing_effect(snapshot);
    _update_block_sparkle(snapshot);
    _backdrop.start(snapshot.presentation_camera_y, _background_clock_ms, true, _visual_theme);
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
    const bool life_indicator_changed = _life_indicator_animation.advance(delta_ms, snapshot.chances_left);
    const bool floor_added = snapshot.floor_count > before.floor_count;
    if(_perfect_landing_elapsed_ms >= 0)
    {
        _perfect_landing_elapsed_ms += delta_ms;
        if(_perfect_landing_elapsed_ms > perfect_landing_star_duration_ms)
        {
            _perfect_landing_elapsed_ms = -1;
            _perfect_landing_floor_index = -1;
        }
    }
    if(floor_added && snapshot.last_accuracy == TowerConstructionAccuracyBand::Perfect &&
       ! _construction.floor(snapshot.floor_count - 1).roof)
    {
        _perfect_landing_elapsed_ms = 0;
        _perfect_landing_floor_index = snapshot.floor_count - 1;
        const TowerConstructionFloor& landed = _construction.floor(_perfect_landing_floor_index);
        _perfect_landing_seed = ((snapshot.floor_count * 19) + (before.floor_count * 13) +
                ((landed.offset < 0 ? -landed.offset : landed.offset) * 3) + (_background_clock_ms / 17)) & 31;
    }
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

    // Release the special-crane OBJ palette before allocating the falling/current
    // block palette. On the first release frame both otherwise coexist briefly,
    // which can exhaust the GBA's 16 BPP4 OBJ palette banks for some block colors.
    const CranePresentationMode presentation_mode = crane_presentation_mode(
            snapshot.status == TowerConstructionStatus::Playing, snapshot.floor_count,
            snapshot.roof_phase, snapshot.block_state == TowerConstructionBlockState::Falling,
            snapshot.block_state == TowerConstructionBlockState::Missed);
    if(presentation_mode != CranePresentationMode::Special)
    {
        _special_cable_sprites.clear();
        _special_boom_sprites.clear();
    }

    // Perfect-landed blocks can leave 32 temporary star/trail sprites alive.
    // Release these cosmetic sprites BEFORE creating the next block, crane,
    // tower floors and HUD; otherwise a new landing can briefly exhaust the
    // GBA's 128 hardware sprite items even though those effects will be
    // regenerated later in this same frame.
    _perfect_star_sprites.clear();
    _block_sparkle_sprites.clear();
    if(snapshot.floor_count != _rendered_floor_count)
    {
        _rebuild_floor_sprites();
    }
    _rebuild_current_sprites(snapshot);
    _ensure_crane_sprites(snapshot);
    _background_clock_ms += delta_ms;
    _update_world_positions(snapshot);
    if(workers_advanced || floor_added)
    {
        _rebuild_worker_sprites(snapshot);
    }

    const int current_combo_bucket = combo_bucket(snapshot);
    const int current_combo_bonus_bucket = combo_bonus_blink_bucket(snapshot);
    const int current_roof_blink_bucket = snapshot.roof_phase ? (_background_clock_ms / 100) & 1 : 0;
    if(life_indicator_changed || snapshot.floor_count != _last_hud_floor_count || snapshot.chances_left != _last_hud_chances ||
       snapshot.population != _last_hud_population || snapshot.combo_count != _last_hud_combo_count ||
       current_combo_bucket != _last_hud_combo_bucket || current_combo_bonus_bucket != _last_hud_combo_bonus_bucket ||
       current_roof_blink_bucket != _last_hud_roof_blink_bucket ||
       snapshot.roof_phase != _last_hud_roof_phase || snapshot.roof_result != _last_hud_roof_result ||
       snapshot.status != _last_hud_status)
    {
        _rebuild_hud(snapshot);
    }
    // Gameplay HUD and combo meter take priority over purely decorative effects.
    // Reconstruct visual effects only after all required sprite items are owned.
    _update_combo_meter(snapshot);
    _update_perfect_landing_effect(snapshot);
    _update_block_sparkle(snapshot);
    _backdrop.update(snapshot.presentation_camera_y, _background_clock_ms);
    return result;
}

bool TowerConstructionScene::active() const
{
    return _active;
}

void TowerConstructionScene::suspend_presentation()
{
    _backdrop.reset();
    _floor_affine_mats.clear();
    _floor_sprites.clear();
    _current_sprites.clear();
    _platform_sprites.clear();
    _crane_hook_sprites.clear();
    _special_cable_sprites.clear();
    _special_boom_sprites.clear();
    _worker_sprites.clear();
    _hud_sprites.clear();
    _combo_meter_fill_sprite.reset();
    _combo_meter_flash_sprite.reset();
    _perfect_seam_sprite.reset();
    _perfect_seam_phase = PerfectLandingSeamPhase::Hidden;
    _perfect_star_sprites.clear();
    _combo_star_sprites.clear();
    _combo_star_frame = -1;
    _block_sparkle_sprites.clear();
    _rendered_current_mesh_id = -1;
    _rendered_tumble_stage = 0;
    _rendered_crane_mesh_id = -1;
    _rendered_crane_rotation_step = 999;
}

void TowerConstructionScene::resume_presentation(const SaveData& save)
{
    if(! _active)
    {
        return;
    }
    set_gameplay_backdrop();
    _visual_theme = visual_theme(save);
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
    _last_hud_combo_bonus_bucket = -1;
    _combo_star_frame = -1;
    _last_hud_roof_blink_bucket = -1;
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
    _update_combo_meter(snapshot);
    _update_perfect_landing_effect(snapshot);
    _update_block_sparkle(snapshot);
    _backdrop.start(snapshot.presentation_camera_y, _background_clock_ms, false, _visual_theme);
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
        const generated::MeshAsset& mesh = mesh_by_id(_visual_theme, mesh_id);
        // Affine matrices are another finite OBJ resource. A temporary
        // allocation failure should skip the affected floor draw and retry.
        if(_floor_affine_mats.size() >= _floor_affine_mats.max_size())
        {
            _floor_sprites.clear();
            _floor_affine_mats.clear();
            _rendered_floor_count = -1;
            return;
        }
        bn::optional<bn::sprite_affine_mat_ptr> affine_mat =
                bn::sprite_affine_mat_ptr::create_optional();
        if(! affine_mat)
        {
            _floor_sprites.clear();
            _floor_affine_mats.clear();
            _rendered_floor_count = -1;
            return;
        }
        _floor_affine_mats.push_back(*affine_mat);
        for(int part_index = 0; part_index < mesh.part_count; ++part_index)
        {
            bn::optional<bn::sprite_ptr> sprite = mesh.parts[part_index].item->create_sprite_optional(0, 0);
            if(! sprite || _floor_sprites.size() >= _floor_sprites.max_size())
            {
                _floor_sprites.clear();
                _floor_affine_mats.clear();
                _rendered_floor_count = -1; // Retry after the next core::update().
                return;
            }
            sprite->set_affine_mat(*affine_mat);
            _floor_sprites.push_back(*sprite);
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
    if(! tumble_pose_available(_visual_theme, mesh_id))
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
            const generated::TumblePoseAsset& pose = tumble_pose_for(
                    _visual_theme, mesh_id, tumble_stage, z_negative, y_negative);
            for(int part_index = 0; part_index < pose.part_count; ++part_index)
            {
                bn::optional<bn::sprite_ptr> sprite = pose.parts[part_index].item->create_sprite_optional(0, 0);
                if(! sprite || _current_sprites.size() >= _current_sprites.max_size())
                {
                    _current_sprites.clear();
                    _rendered_current_mesh_id = -1; // Retry the pose next frame.
                    return;
                }
                sprite->set_z_order(current_block_z_order);
                _current_sprites.push_back(*sprite);
            }
        }
        else
        {
            create_mesh_sprites(mesh_by_id(_visual_theme, mesh_id), _current_sprites);
            for(bn::sprite_ptr& sprite : _current_sprites)
            {
                sprite.set_affine_mat(_current_affine_mat);
                sprite.set_z_order(current_block_z_order);
            }
        }
        const int required_parts = tumble_stage > 0 ?
                tumble_pose_for(_visual_theme, mesh_id, tumble_stage, z_negative, y_negative).part_count :
                mesh_by_id(_visual_theme, mesh_id).part_count;
        if(_current_sprites.size() != required_parts)
        {
            _rendered_current_mesh_id = -1;
            return;
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
        create_mesh_sprites(mesh_by_id(_visual_theme, platform_mesh_id), _platform_sprites);
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
            create_mesh_sprites(mesh_by_id(_visual_theme, special_crane_mesh_id), _crane_hook_sprites);
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
    const generated::CraneHookFrameAsset& frame = crane_hook_frame_for_step(_visual_theme, rotation_step);
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
        _special_boom_sprites.clear();
        return;
    }

    // House.e(Graphics) starts this Java2D line at the screen centre in X.
    // Its source camera anchor is 1920 fixed units above the active camera:
    // -(22 * 1920 >> 8) = -165 in Butano's screen-centred coordinates.
    constexpr int start_x = 0;
    const bool intro_camera_active = snapshot.floor_count == 0 &&
            snapshot.block_state == TowerConstructionBlockState::Attached && snapshot.camera_y != snapshot.camera_target_y;
    const int start_y = special_crane_cable_start_y(snapshot.presentation_camera_y, intro_camera_active);
    constexpr int endpoint_overlap = 3;
    const int end_x = _screen_x(snapshot.crane_x);
    const int end_y = _screen_y(snapshot.crane_y + 528, snapshot.presentation_camera_y);
    const int dx = end_x - start_x;
    const int dy = end_y - start_y;
    const int cable_length = bn::sqrt(dx * dx + dy * dy);
    if(cable_length <= 0)
    {
        _special_cable_sprites.clear();
        _special_boom_sprites.clear();
        return;
    }

    if(_special_cable_sprites.empty())
    {
        bn::optional<bn::sprite_ptr> sprite =
                bn::sprite_items::crane_special_cable_segment.create_sprite_optional(0, 0);
        if(! sprite) { return; }
        sprite->set_z_order(special_cable_z_order);
        sprite->set_double_size_mode(bn::sprite_double_size_mode::ENABLED);
        _special_cable_sprites.push_back(*sprite);
    }

    if(_special_boom_sprites.empty())
    {
        bn::optional<bn::sprite_ptr> p0 = bn::sprite_items::crane_special_boom_p0.create_sprite_optional(0, 0);
        bn::optional<bn::sprite_ptr> p1 = bn::sprite_items::crane_special_boom_p1.create_sprite_optional(0, 0);
        bn::optional<bn::sprite_ptr> p2 = bn::sprite_items::crane_special_boom_p2.create_sprite_optional(0, 0);
        if(p0 && p1 && p2)
        {
            p0->set_z_order(special_boom_z_order);
            p1->set_z_order(special_boom_z_order);
            p2->set_z_order(special_boom_z_order);
            _special_boom_sprites.push_back(*p0);
            _special_boom_sprites.push_back(*p1);
            _special_boom_sprites.push_back(*p2);
        }
    }
    const int boom_y = special_crane_boom_center_y(start_y);
    for(int part_index = 0; part_index < _special_boom_sprites.size(); ++part_index)
    {
        _special_boom_sprites[part_index].set_position(
                special_crane_boom_part_center_x(start_x, part_index), boom_y);
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
        const generated::MeshAsset& mesh = mesh_by_id(_visual_theme, mesh_id);
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
    if(current_visible && ! _current_sprites.empty() && _rendered_current_mesh_id >= 0)
    {
        const int x = _screen_x(snapshot.current_x);
        const int y = _screen_y(snapshot.current_y, snapshot.presentation_camera_y);
        if(_rendered_tumble_stage > 0)
        {
            const generated::TumblePoseAsset& pose = tumble_pose_for(
                    _visual_theme, _rendered_current_mesh_id, _rendered_tumble_stage,
                    _rendered_tumble_z_negative, _rendered_tumble_y_negative);
            for(int part_index = 0; part_index < pose.part_count; ++part_index)
            {
                _current_sprites[part_index].set_position(
                        x + pose.parts[part_index].x, y + pose.parts[part_index].y);
            }
        }
        else
        {
            const generated::MeshAsset& mesh = mesh_by_id(_visual_theme, _rendered_current_mesh_id);
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
        position_mesh_sprites(mesh_by_id(_visual_theme, platform_mesh_id), _screen_x(0),
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
                    mesh_by_id(_visual_theme, special_crane_mesh_id), crane_x, crane_y, _crane_hook_sprites);
        }
        else
        {
            const generated::CraneHookFrameAsset& frame =
                    crane_hook_frame_for_step(_visual_theme, snapshot.crane_x >> 4);
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
        const generated::UiCompositeAsset& asset = gameplay_worker_asset(worker);
        show_ui_composite(
                asset, _screen_x(worker.x_fixed),
                _screen_y(worker.y_fixed, snapshot.presentation_camera_y), _worker_sprites,
                gameplay_worker_z_order);
    }
}

void TowerConstructionScene::_update_combo_meter(const TowerConstructionSnapshot& snapshot)
{
    if(snapshot.status != TowerConstructionStatus::Playing || snapshot.combo_meter_ms <= 0)
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
        bn::optional<bn::sprite_ptr> sprite = part.item->create_sprite_optional(0, 0);
        if(! sprite) { return; }
        sprite->set_z_order(-100);
        sprite->set_double_size_mode(bn::sprite_double_size_mode::ENABLED);
        active = *sprite;
    }

    bn::sprite_ptr& sprite = *active;
    const int part_y = flash ? generated::quick_combo_meter_flash.parts[0].y :
                               generated::quick_combo_meter_fill.parts[0].y;
    sprite.set_horizontal_scale(bn::fixed(width) / 64);
    sprite.set_position(bn::fixed(combo_meter_fill_left * 2 + width) / 2, combo_meter_fill_y + part_y);

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

void TowerConstructionScene::_update_block_sparkle(const TowerConstructionSnapshot& snapshot)
{
    _block_sparkle_sprites.clear();
    if(snapshot.status != TowerConstructionStatus::Playing || snapshot.combo_count <= 0 || snapshot.floor_count <= 0)
    {
        return;
    }

    int first_floor = snapshot.floor_count - snapshot.combo_count;
    if(first_floor < _visible_floor_start)
    {
        first_floor = _visible_floor_start;
    }
    if(first_floor < 0)
    {
        first_floor = 0;
    }

    const int animation_bucket = _background_clock_ms / 100;
    for(int floor_index = snapshot.floor_count - 1; floor_index >= first_floor; --floor_index)
    {
        const TowerConstructionFloor& floor = _construction.floor(floor_index);
        const int visible_slot = floor_index - _visible_floor_start;
        const int frame = (animation_bucket + visible_slot) % 3;
        const generated::UiCompositeAsset& asset = *generated::legacy_block_sparkle_frames[frame];
        if(bn::sprites::available_items_count() >= asset.part_count)
        {
            show_ui_composite(
                    asset, _screen_x(floor.x),
                    _screen_y(floor.y, snapshot.presentation_camera_y),
                    _block_sparkle_sprites, -21);
        }
    }

    const bool current_visible = snapshot.status == TowerConstructionStatus::Playing &&
            (snapshot.block_state == TowerConstructionBlockState::Raising || snapshot.block_state == TowerConstructionBlockState::Attached ||
             snapshot.block_state == TowerConstructionBlockState::Falling || snapshot.block_state == TowerConstructionBlockState::Slipping);
    if(current_visible)
    {
        const generated::UiCompositeAsset& asset = *generated::legacy_block_sparkle_frames[animation_bucket % 3];
        if(bn::sprites::available_items_count() >= asset.part_count)
        {
            show_ui_composite(asset, _screen_x(snapshot.current_x),
                              _screen_y(snapshot.current_y, snapshot.presentation_camera_y),
                              _block_sparkle_sprites, -19);
        }
    }
}

void TowerConstructionScene::_update_perfect_landing_effect(const TowerConstructionSnapshot& snapshot)
{
    _perfect_star_sprites.clear();
    if(_perfect_landing_elapsed_ms < 0 || snapshot.status != TowerConstructionStatus::Playing ||
       _perfect_landing_floor_index < 0 || _perfect_landing_floor_index >= snapshot.floor_count)
    {
        _perfect_seam_sprite.reset();
        _perfect_seam_phase = PerfectLandingSeamPhase::Hidden;
        return;
    }

    const TowerConstructionFloor& floor = _construction.floor(_perfect_landing_floor_index);
    if(floor.roof)
    {
        _perfect_seam_sprite.reset();
        _perfect_seam_phase = PerfectLandingSeamPhase::Hidden;
        return;
    }

    const TowerConstructionRenderPose& pose = _construction.floor_render_pose(_perfect_landing_floor_index);
    const int x = _screen_x(floor.x + pose.x_delta);
    const int y = _screen_y(floor.y + pose.y_delta, snapshot.presentation_camera_y);

    // JAR-grounded pass: the moving star grows small -> medium -> large while
    // leaving an actual sampled trail behind it (white -> yellow -> red).
    const generated::UiCompositeAsset& head_asset = _perfect_landing_elapsed_ms < 120 ?
            generated::accuracy_star_f1 : _perfect_landing_elapsed_ms < 240 ?
            generated::accuracy_star_f2 : generated::accuracy_star_f0;

    // Reserve one sprite item for the landing seam before allocating optional
    // trails. Four star heads have higher priority than trail samples, so a
    // crowded scene preserves the original four-star burst and degrades only
    // the number of dots in each trailing line.
    const PerfectLandingSeamPhase phase = perfect_landing_seam_phase(_perfect_landing_elapsed_ms);
    const int seam_reserve = phase != PerfectLandingSeamPhase::Hidden ? 1 : 0;
    for(int index = 0; index < perfect_landing_star_count; ++index)
    {
        if(bn::sprites::available_items_count() < head_asset.part_count + seam_reserve)
        {
            break;
        }
        const int star_x = x + perfect_landing_star_offset_x(index, _perfect_landing_elapsed_ms, _perfect_landing_seed);
        const int star_y = y + perfect_landing_star_offset_y(index, _perfect_landing_elapsed_ms, _perfect_landing_seed);
        show_ui_composite(head_asset, star_x, star_y, _perfect_star_sprites, -18);
    }

    // Add trail samples by age across all four stars. Each sample is optional;
    // a full 128-sprite OAM must never turn perfect placement into a crash.
    for(int trail_index = 0; trail_index < perfect_landing_trail_sample_count; ++trail_index)
    {
        const int trail_elapsed = perfect_landing_star_trail_elapsed(_perfect_landing_elapsed_ms, trail_index);
        if(trail_elapsed <= 0)
        {
            continue;
        }
        const generated::UiCompositeAsset& trail_asset = trail_index < 2 ? generated::accuracy_trail_white :
                                                         trail_index < 4 ? generated::accuracy_trail_yellow :
                                                                           generated::accuracy_trail_red;
        for(int index = 0; index < perfect_landing_star_count; ++index)
        {
            if(bn::sprites::available_items_count() < trail_asset.part_count + seam_reserve)
            {
                break;
            }
            const int trail_x = x + perfect_landing_star_offset_x(index, trail_elapsed, _perfect_landing_seed);
            const int trail_y = y + perfect_landing_star_offset_y(index, trail_elapsed, _perfect_landing_seed);
            show_ui_composite(trail_asset, trail_x, trail_y, _perfect_star_sprites, -24 + trail_index);
        }
    }

    if(phase == PerfectLandingSeamPhase::Hidden)
    {
        _perfect_seam_sprite.reset();
        _perfect_seam_phase = phase;
        return;
    }

    const generated::UiCompositeAsset& seam_asset = phase == PerfectLandingSeamPhase::White ?
            generated::combo_seam_flash_white : generated::combo_seam_flash;
    const generated::UiSpritePartAsset& part = seam_asset.parts[0];
    const int seam_y = y + pixels_per_floor / 2;
    if(! _perfect_seam_sprite || phase != _perfect_seam_phase)
    {
        _perfect_seam_sprite.reset();
        bn::optional<bn::sprite_ptr> sprite = part.item->create_sprite_optional(x + part.x, seam_y + part.y);
        if(! sprite) { return; }
        sprite->set_z_order(-22);
        _perfect_seam_sprite = *sprite;
        _perfect_seam_phase = phase;
    }
    else
    {
        (*_perfect_seam_sprite).set_position(x + part.x, seam_y + part.y);
    }
}

void TowerConstructionScene::_rebuild_hud(const TowerConstructionSnapshot& snapshot)
{
    _hud_sprites.clear();
    _last_hud_floor_count = snapshot.floor_count;
    _last_hud_chances = snapshot.chances_left;
    _last_hud_population = snapshot.population;
    _last_hud_combo_count = snapshot.combo_count;
    _last_hud_combo_bucket = combo_bucket(snapshot);
    _last_hud_combo_bonus_bucket = combo_bonus_blink_bucket(snapshot);
    _last_hud_roof_blink_bucket = snapshot.roof_phase ? (_background_clock_ms / 100) & 1 : 0;
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
        const int frame = _life_indicator_animation.frame_for_slot(slot, snapshot.chances_left, active_frame);
        const int top_y = 132 + slot * 6;
        show_ui_composite(*construction_state_indicator_frames[frame], -92, top_y + 3 - 80, _hud_sprites);
    }

    // B==3 construction branch: resource 13 selects frame L-1 and is placed
    // at x=au-2, y=c-av-12-2*J. J is the target floor count (10/20/30/40).
    const int badge_top_y = 160 - 10 - 12 - 2 * snapshot.target_height;
    const generated::UiCompositeAsset& badge =
            (snapshot.roof_phase && _last_hud_roof_blink_bucket != 0) ?
            generated::construction_target_badge_f4 : *construction_target_badge_frames[building_index];
    show_ui_composite(badge, -105, badge_top_y + 6 - 80, _hud_sprites);

    // The population marker/digits are part of the common House HUD and only
    // appear after at least one floor has landed.
    if(snapshot.floor_count > 0)
    {
        show_ui_composite(generated::hud_population_icon, 82, 63, _hud_sprites);
        draw_source_number(snapshot.population, 5, 228, 141, construction_white_digit_frames, _hud_sprites);
    }

    if(snapshot.combo_meter_ms > 0)
    {
        show_ui_composite(
                generated::quick_combo_meter_frame, combo_meter_frame_x, combo_meter_frame_y,
                _hud_sprites, -101);
    }

    if(snapshot.combo_meter_ms > 0 && snapshot.combo_count > 1)
    {
        show_ui_composite(*hud_brown_digit_frames[11], combo_readout_x, combo_readout_y, _hud_sprites);
        const int digits = snapshot.combo_count > 9 ? 2 : 1;
        draw_source_number(snapshot.combo_count, digits, 191 + digits * 5, 12,
                           hud_brown_digit_frames, _hud_sprites);
    }

    if(snapshot.combo_bonus_pending != 0 && snapshot.combo_meter_ms <= 0 &&
       snapshot.combo_meter_ms > -2000 && (-snapshot.combo_meter_ms / 100) % 2 == 0)
    {
        const int plus_left = draw_source_number(snapshot.combo_bonus_pending, 3, 130, 10,
                                                 hud_brown_digit_frames, _hud_sprites);
        show_ui_composite(*hud_brown_digit_frames[10], plus_left + 2 - 120, 13 - 80, _hud_sprites);
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
    int y = -((line_count - 1) * modal_line_spacing) / 2;
    for(int line = 0; line < line_count; ++line)
    {
        const bn::string<128> formatted = format_construction_modal_line(
                generated::city_modal_lines[_language][modal_index][line]);
        (void) _text_generator.generate_optional(0, y, formatted, _hud_sprites);
        y += modal_line_spacing;
    }
    show_ui_composite(generated::support_nav_f2, 0, 64, _hud_sprites, -100);
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
