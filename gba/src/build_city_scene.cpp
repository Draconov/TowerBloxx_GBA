#include "tb/build_city_scene.h"

#include "tb/scene_backdrop.h"
#include "tb/build_city_visuals.h"
#include "tb/christmas_city_assets.h"
#include "tb/christmas_hud_assets.h"

#include "bn_bg_palettes.h"
#include "bn_color.h"
#include "bn_sprites.h"
#include "bn_sprite_palette_ptr.h"
#include "bn_string.h"
#include "bn_string_view.h"
#include "bn_regular_bg_items_city_bg_theme_0.h"
#include "bn_regular_bg_items_city_bg_theme_1.h"
#include "bn_regular_bg_items_city_bg_theme_2.h"
#include "bn_regular_bg_items_city_bg_theme_3.h"
#include "bn_regular_bg_items_christmas_city_bg_theme_0.h"
#include "bn_regular_bg_items_christmas_city_bg_theme_1.h"
#include "bn_regular_bg_items_christmas_city_bg_theme_2.h"
#include "bn_regular_bg_items_christmas_city_bg_theme_3.h"

#include "generated/tower_localization.h"
#include "generated/tower_ui_assets.h"

namespace tb
{
namespace
{
// Exact 240x160 specialization of m.a(Graphics, boolean). The regular
// background contains the Java2D board/roads/selector shell; dynamic source
// sprites use these same pixel anchors.
constexpr int screen_center_x = 120;
constexpr int screen_center_y = 80;
constexpr int grid_screen_left = 89;
constexpr int grid_screen_top = 32;
constexpr int grid_spacing = 17;
constexpr int cell_screen_left = grid_screen_left + 3;
constexpr int cell_screen_top = grid_screen_top + 3;
constexpr int selector_screen_left = grid_screen_left - 26;
constexpr int selector_screen_top = grid_screen_top + 2;

constexpr int building_widths[4] = {15, 16, 17, 19};
constexpr int building_heights[4] = {14, 16, 17, 19};
constexpr int trophy_population_thresholds[4] = {70, 250, 550, 1000};
constexpr int modal_backdrop_z_order = -90;
constexpr int modal_line_spacing = 12;
// A valid-lot pulse is a floor marker, not an overlay on placed buildings.
// In Butano lower sprite z-order values draw in front of higher values.
constexpr int valid_lot_ring_z_order = 1;
constexpr int selected_preview_outline_z_order = -2;
constexpr int selected_preview_tower_z_order = -3;
constexpr int placement_pending_z_order = -4;
constexpr int discard_effect_z_order = -5;
constexpr int discard_cell_center_x = 72;
constexpr int discard_cell_center_y = 117;
constexpr int discard_building_center_x = discard_cell_center_x + 2;
constexpr int discard_building_center_y = discard_cell_center_y - 2;

const generated::UiCompositeAsset* const building_assets[4][4] = {
    { &generated::city_building_1_f0, &generated::city_building_1_f1,
      &generated::city_building_1_f2, &generated::city_building_1_f3 },
    { &generated::city_building_2_f0, &generated::city_building_2_f1,
      &generated::city_building_2_f2, &generated::city_building_2_f3 },
    { &generated::city_building_3_f0, &generated::city_building_3_f1,
      &generated::city_building_3_f2, &generated::city_building_3_f3 },
    { &generated::city_building_4_f0, &generated::city_building_4_f1,
      &generated::city_building_4_f2, &generated::city_building_4_f3 },
};

const generated::UiCompositeAsset* const lot_assets[5] = {
    &generated::city_lot_f0,
    &generated::city_lot_f1,
    &generated::city_lot_f2,
    &generated::city_lot_f3,
    &generated::city_lot_f4,
};

const generated::UiCompositeAsset* const brown_digits[10] = {
    &generated::hud_brown_digit_f0,
    &generated::hud_brown_digit_f1,
    &generated::hud_brown_digit_f2,
    &generated::hud_brown_digit_f3,
    &generated::hud_brown_digit_f4,
    &generated::hud_brown_digit_f5,
    &generated::hud_brown_digit_f6,
    &generated::hud_brown_digit_f7,
    &generated::hud_brown_digit_f8,
    &generated::hud_brown_digit_f9,
};

const generated::UiCompositeAsset* const red_digits[10] = {
    &generated::hud_red_digit_f0,
    &generated::hud_red_digit_f1,
    &generated::hud_red_digit_f2,
    &generated::hud_red_digit_f3,
    &generated::hud_red_digit_f4,
    &generated::hud_red_digit_f5,
    &generated::hud_red_digit_f6,
    &generated::hud_red_digit_f7,
    &generated::hud_red_digit_f8,
    &generated::hud_red_digit_f9,
};

const generated::UiCompositeAsset* const white_digits[10] = {
    &generated::hud_white_digit_f0,
    &generated::hud_white_digit_f1,
    &generated::hud_white_digit_f2,
    &generated::hud_white_digit_f3,
    &generated::hud_white_digit_f4,
    &generated::hud_white_digit_f5,
    &generated::hud_white_digit_f6,
    &generated::hud_white_digit_f7,
    &generated::hud_white_digit_f8,
    &generated::hud_white_digit_f9,
};

const generated::UiCompositeAsset* const city_effects[6] = {
    &generated::city_effect_f0,
    &generated::city_effect_f1,
    &generated::city_effect_f2,
    &generated::city_effect_f3,
    &generated::city_effect_f4,
    &generated::city_effect_f5,
};

const generated::UiCompositeAsset* const city_panel_states[4] = {
    &generated::city_status_panel_f0,
    &generated::city_status_panel_f1,
    &generated::city_status_panel_f2,
    &generated::city_status_panel_f3,
};

const generated::UiCompositeAsset* const city_type_badges[4] = {
    &generated::city_type_badge_1,
    &generated::city_type_badge_2,
    &generated::city_type_badge_3,
    &generated::city_type_badge_4,
};

const generated::UiCompositeAsset* const city_level_icons[9] = {
    &generated::city_level_icon_f0,
    &generated::city_level_icon_f1,
    &generated::city_level_icon_f2,
    &generated::city_level_icon_f3,
    &generated::city_level_icon_f4,
    &generated::city_level_icon_f5,
    &generated::city_level_icon_f6,
    &generated::city_level_icon_f7,
    &generated::city_level_icon_f8,
};

const generated::UiCompositeAsset* const city_progress_tails[7] = {
    &generated::city_progress_tail_f1,
    &generated::city_progress_tail_f2,
    &generated::city_progress_tail_f3,
    &generated::city_progress_tail_f4,
    &generated::city_progress_tail_f5,
    &generated::city_progress_tail_f6,
    &generated::city_progress_tail_f7,
};

const generated::UiCompositeAsset& building_asset(int type, int roof, GameTheme theme)
{
    if(theme == GameTheme::Christmas)
    {
        return christmas::city_building_asset(type, roof);
    }
    if(type < 1) { type = 1; }
    if(type > 4) { type = 4; }
    if(roof < 0) { roof = 0; }
    if(roof > 3) { roof = 3; }
    return *building_assets[type - 1][roof];
}

const generated::UiCompositeAsset& lot_asset(int frame, GameTheme theme)
{
    if(frame < 0) { frame = 0; }
    if(frame > 4) { frame = 4; }
    return theme == GameTheme::Christmas ? *christmas::city_lots[frame] : *lot_assets[frame];
}

const generated::UiCompositeAsset& city_effect_asset(int frame, bool replacing, GameTheme theme)
{
    if(frame < 0) { frame = 0; }
    if(frame > 5) { frame = 5; }
    if(theme == GameTheme::Christmas)
    {
        return replacing ? *christmas::city_effects_replace[frame] : *christmas::city_effects_empty[frame];
    }
    return *city_effects[frame];
}

const generated::UiCompositeAsset& city_level_icon_asset(int index, GameTheme theme)
{
    if(index < 0) { index = 0; }
    if(index > 8) { index = 8; }
    return theme == GameTheme::Christmas ? *christmas::city_level_icons[index] : *city_level_icons[index];
}

int centered_x(int screen_x)
{
    return screen_x - screen_center_x;
}

int centered_y(int screen_y)
{
    return screen_y - screen_center_y;
}

void show_sprite_item(
        bn::ivector<bn::sprite_ptr>& sprites, const bn::sprite_item& item, int graphics_index,
        int screen_x, int screen_y, int z_order = 0)
{
    if(sprites.size() >= sprites.max_size() || bn::sprites::available_items_count() <= 0)
    {
        return;
    }
    bn::optional<bn::sprite_ptr> sprite = item.create_sprite_optional(
            centered_x(screen_x), centered_y(screen_y), graphics_index);
    if(sprite)
    {
        sprite->set_z_order(z_order);
        sprites.push_back(*sprite);
    }
}

int building_center_x(int type, int screen_left)
{
    return centered_x(screen_left + building_widths[type - 1] / 2);
}

int building_center_y(int type, int screen_baseline)
{
    const int height = building_heights[type - 1];
    return centered_y(screen_baseline - height + height / 2);
}

bn::string<96> flatten_city_instruction(bn::string_view template_text, int value)
{
    bn::string<96> result;
    for(int index = 0; index < template_text.size(); ++index)
    {
        if(template_text[index] == '%' && index + 1 < template_text.size() && template_text[index + 1] == 'U')
        {
            result.append(bn::to_string<12>(value));
            ++index;
        }
        else if(template_text[index] == '\\' && index + 1 < template_text.size() && template_text[index + 1] == 'n')
        {
            if(! result.empty() && result[result.size() - 1] != ' ')
            {
                result.append(' ');
            }
            ++index;
        }
        else
        {
            result.append(template_text[index]);
        }
    }
    return result;
}

bn::string<96> format_trophy_instruction(
        bn::string_view template_text, bn::string_view tower_name, int population)
{
    bn::string<96> result;
    for(int index = 0; index < template_text.size(); ++index)
    {
        if(template_text[index] == '%' && index + 2 < template_text.size() &&
           template_text[index + 2] == 'U')
        {
            if(template_text[index + 1] == '0')
            {
                result.append(tower_name);
                index += 2;
                continue;
            }
            if(template_text[index + 1] == '1')
            {
                result.append(bn::to_string<12>(population));
                index += 2;
                continue;
            }
        }
        result.append(template_text[index]);
    }
    return result;
}

bn::string<128> format_event_line(
        bn::string_view template_text, const BuildCityEvent& event, int language)
{
    bn::string<128> result;
    for(int index = 0; index < template_text.size(); ++index)
    {
        if(template_text[index] != '%')
        {
            result.append(template_text[index]);
            continue;
        }

        if(index + 2 < template_text.size() && template_text[index + 2] == 'U' &&
           (template_text[index + 1] == '0' || template_text[index + 1] == '1'))
        {
            const int argument = template_text[index + 1] - '0';
            const int text_index = argument == 0 ? event.text_index0 : event.text_index1;
            if(text_index >= 0)
            {
                result.append(generated::localized_strings[language][text_index]);
            }
            index += 2;
            continue;
        }

        if(index + 1 < template_text.size() && template_text[index + 1] == 'U')
        {
            if(event.argument_kind == BuildCityEventArgumentKind::Number)
            {
                result.append(bn::to_string<16>(event.number_value));
            }
            else if(event.argument_kind == BuildCityEventArgumentKind::Text && event.text_index0 >= 0)
            {
                result.append(generated::localized_strings[language][event.text_index0]);
            }
            index += 1;
            continue;
        }

        result.append(template_text[index]);
    }
    return result;
}

void show_comparison_digits(
        bn::ivector<bn::sprite_ptr>& sprites,
        const generated::UiCompositeAsset* const* digits,
        int value,
        int right_anchor,
        int screen_y)
{
    if(value < 0)
    {
        value = 0;
    }

    bn::string<12> text = bn::to_string<12>(value);
    const int digit_count = text.size();
    for(int index = digit_count - 1; index >= 0; --index)
    {
        const int digit = text[index] - '0';
        const int from_right = digit_count - 1 - index;
        const int digit_center_x = right_anchor - 3 - from_right * 4;
        const generated::UiCompositeAsset& asset = *digits[digit];
        for(int part_index = 0; part_index < asset.part_count; ++part_index)
        {
            const generated::UiSpritePartAsset& part = asset.parts[part_index];
            if(sprites.size() < sprites.max_size())
            {
                bn::optional<bn::sprite_ptr> sprite = part.item->create_sprite_optional(
                        centered_x(digit_center_x + part.x), centered_y(screen_y + part.y));
                if(sprite) { sprites.push_back(*sprite); }
            }
        }
    }
}

void show_christmas_comparison_digits(
        bn::ivector<bn::sprite_ptr>& sprites, int value, int right_anchor, int screen_y)
{
    if(value < 0)
    {
        value = 0;
    }

    bn::string<12> text = bn::to_string<12>(value);
    const int digit_count = text.size();
    for(int index = digit_count - 1; index >= 0; --index)
    {
        const int digit = text[index] - '0';
        const int from_right = digit_count - 1 - index;
        const int digit_center_x = right_anchor - 3 - from_right * 4;
        show_sprite_item(sprites, christmas::white_digits, digit, digit_center_x + 2, screen_y + 1);
    }
}

BuildCityProgressState progress_state(const BuildCitySnapshot& snapshot)
{
    BuildCityProgressState state;
    state.total_population = snapshot.total_population;
    state.milestone = snapshot.milestone;
    state.city_level = snapshot.city_level;
    state.max_unlocked_building_type = snapshot.max_unlocked_building_type;
    state.max_trophy_building_type = snapshot.max_trophy_building_type;
    state.occupied_tiles = snapshot.occupied_tiles;
    return state;
}

bool snapshot_changed(const BuildCitySnapshot& left, const BuildCitySnapshot& right)
{
    return left.mode != right.mode || left.sandbox_active != right.sandbox_active ||
           left.total_population != right.total_population ||
           left.milestone != right.milestone || left.city_level != right.city_level ||
           left.max_unlocked_building_type != right.max_unlocked_building_type ||
           left.max_trophy_building_type != right.max_trophy_building_type ||
           left.selected_building_type != right.selected_building_type ||
           left.selected_unlock_population != right.selected_unlock_population ||
           left.construction_select_ms != right.construction_select_ms ||
           left.cursor_column != right.cursor_column || left.cursor_row != right.cursor_row ||
           left.placement_valid != right.placement_valid ||
           left.placement_committing != right.placement_committing ||
           left.placement_timer_ms != right.placement_timer_ms ||
           left.pending_building_type != right.pending_building_type ||
           left.pending_population != right.pending_population || left.pending_roof != right.pending_roof ||
           left.last_population_delta != right.last_population_delta ||
           left.occupied_tiles != right.occupied_tiles ||
           left.current_milestone_population != right.current_milestone_population ||
           left.next_milestone_population != right.next_milestone_population ||
           left.placement_transition_ms != right.placement_transition_ms ||
           left.replacement_population != right.replacement_population ||
           left.placement_capabilities != right.placement_capabilities;
}
}

BuildCityScene::BuildCityScene(const SaveData& save) :
    _city(save),
    _text_generator(generated::tower_font)
{
    _text_generator.set_center_alignment();
    _text_generator.set_z_order(-100);
}

void BuildCityScene::start(const SaveData& save, int language, GameTheme theme)
{
    set_city_backdrop();
    _city.reset_from_save(save);
    const BuildCitySnapshot initial_snapshot = _city.snapshot();
    _display_population = initial_snapshot.total_population;
    _population_roll.reset();
    _rendered_city_theme = -1;
    _events.clear_runtime();
    _committed_progress = progress_state(_city.snapshot());
    _events.on_city_entered(_committed_progress, save);
    _placement_score_pending = false;
    _deferred_placement_score = 0;
    _language = language;
    _theme = theme;
    _frame_phase = 0;
    _placement_flash_ms = 0;
    _selector_flash_ms = 0;
    _has_snapshot = false;
    _active = true;
    _rebuild(save);
}

BuildCitySceneUpdateResult BuildCityScene::update(const InputFrame& input, SaveData& save)
{
    set_city_backdrop();
    BuildCitySceneUpdateResult result;
    if(! _active)
    {
        result.exit = true;
        return result;
    }

    if(_events.has_event())
    {
        if(input.pressed(Key::A) || input.pressed(Key::Start))
        {
            result.save_dirty = _events.acknowledge(save);
            if(! _events.has_event() && _placement_score_pending)
            {
                result.placement_committed = ! _city.sandbox_active();
                result.committed_total_population = _deferred_placement_score;
                _placement_score_pending = false;
                _deferred_placement_score = 0;
            }
            _rebuild(save);
        }
        return result;
    }

    const BuildCityConstructionRequest request_before = _city.construction_request();
    if(request_before.pending)
    {
        result.construction_requested = true;
        if(input.pressed(Key::B))
        {
            _city.clear_construction_request();
            _rebuild(save);
            result.construction_requested = false;
        }
        return result;
    }

    static constexpr int frame_deltas[] = {16, 17, 17};
    const int delta_ms = frame_deltas[_frame_phase];
    _frame_phase = (_frame_phase + 1) % 3;
    const BuildCityUpdateResult city_result = _city.update(delta_ms, input, save);
    result.exit = city_result.exit;
    result.save_dirty = city_result.save_dirty;
    result.sandbox_activated = city_result.sandbox_activated;
    result.construction_requested = _city.construction_request().pending;

    _population_roll.update(delta_ms);
    const BuildCitySnapshot snapshot = _city.snapshot();
    if(snapshot.total_population != _display_population)
    {
        _population_roll.start(_display_population, snapshot.total_population);
        _display_population = snapshot.total_population;
    }
    if(city_result.placement_committed)
    {
        const BuildCityProgressState after = progress_state(snapshot);
        _events.on_placement_committed(_committed_progress, after, save);
        _committed_progress = after;
        if(_events.has_event())
        {
            _placement_score_pending = true;
            _deferred_placement_score = city_result.committed_total_population;
        }
        else
        {
            result.placement_committed = ! _city.sandbox_active();
            result.committed_total_population = city_result.committed_total_population;
        }
    }

    // m.x is the continuous 800ms valid-lot pulse timer in both browser and
    // placement views. m.t advances only while browsing and flashes the
    // selected 15x15 selector slot orange for the first half of each 500ms.
    _placement_flash_ms = (_placement_flash_ms + delta_ms) % 800;
    if(snapshot.mode == BuildCityMode::Browse)
    {
        _selector_flash_ms = (_selector_flash_ms + delta_ms) % 500;
    }

    if(result.exit)
    {
        _stop();
        return result;
    }

    const bool browse_animation = snapshot.mode == BuildCityMode::Browse;
    const bool placement_animation = snapshot.mode == BuildCityMode::Placement && ! snapshot.placement_committing;
    const bool population_animation = _population_roll.animating();
    if(! _has_snapshot || snapshot_changed(snapshot, _last_snapshot) || city_result.save_dirty ||
       result.construction_requested || _events.has_event() || browse_animation || placement_animation || population_animation)
    {
        _rebuild(save);
    }
    return result;
}

bool BuildCityScene::sandbox_active() const
{
    return _city.sandbox_active();
}

bool BuildCityScene::active() const
{
    return _active;
}

BuildCityConstructionRequest BuildCityScene::construction_request() const
{
    return _city.construction_request();
}

void BuildCityScene::clear_construction_request()
{
    _city.clear_construction_request();
    _sprites.clear();
    _background.reset();
    _rendered_city_theme = -1;
    _has_snapshot = false;
}

void BuildCityScene::accept_constructed_tower(
        uint8_t building_type, int population, uint8_t roof, const SaveData& save)
{
    _city.accept_constructed_tower(building_type, population, roof);
    _events.on_constructed_tower_accepted(progress_state(_city.snapshot()), save);
    _has_snapshot = false;
}

void BuildCityScene::suspend_presentation()
{
    _background.reset();
    _sprites.clear();
    _has_snapshot = false;
}

void BuildCityScene::resume_presentation(const SaveData& save, GameTheme theme)
{
    _theme = theme;
    if(! _active)
    {
        return;
    }
    set_city_backdrop();
    _has_snapshot = false;
    _rebuild(save);
}

void BuildCityScene::_stop()
{
    _active = false;
    _background.reset();
    _sprites.clear();
    _has_snapshot = false;
}

void BuildCityScene::_show_composite(const generated::UiCompositeAsset& asset, int x, int y, int z_order)
{
    if(_sprites.max_size() - _sprites.size() < asset.part_count ||
       bn::sprites::available_items_count() < asset.part_count)
    {
        return;
    }
    const int first = _sprites.size();
    for(int index = 0; index < asset.part_count; ++index)
    {
        const generated::UiSpritePartAsset& part = asset.parts[index];
        bn::optional<bn::sprite_ptr> sprite = part.item->create_sprite_optional(x + part.x, y + part.y);
        if(! sprite)
        {
            while(_sprites.size() > first) { _sprites.pop_back(); }
            return;
        }
        sprite->set_z_order(z_order);
        _sprites.push_back(*sprite);
    }
}

void BuildCityScene::_show_modal_backdrop(int left, int top, int columns, int rows)
{
    (void) left;
    (void) top;
    (void) columns;
    (void) rows;
    _show_composite(generated::dialog_window, 0, 0, modal_backdrop_z_order);
}

void BuildCityScene::_show_valid_lot_ring(
        int screen_x, int screen_y, bn::optional<bn::sprite_palette_ptr>& valid_lot_palette)
{
    const generated::UiCompositeAsset& asset = generated::city_valid_lot_ring;
    for(int index = 0; index < asset.part_count; ++index)
    {
        const generated::UiSpritePartAsset& part = asset.parts[index];
        if(_sprites.size() >= _sprites.max_size()) { return; }
        bn::optional<bn::sprite_ptr> sprite = part.item->create_sprite_optional(
                centered_x(screen_x + part.x), centered_y(screen_y + part.y));
        if(! sprite) { return; }
        if(! valid_lot_palette)
        {
            valid_lot_palette = sprite->palette();
        }
        // Keep the pulsing floor marker beneath any existing tower, including
        // when the occupied cell is the selected replacement destination.
        sprite->set_z_order(valid_lot_ring_z_order);
        _sprites.push_back(*sprite);
    }
}

void BuildCityScene::_show_city_tiles(const SaveData& save, const BuildCitySnapshot& snapshot)
{
    int pulse_building_type = 0;
    if(! _events.has_event())
    {
        if(snapshot.mode == BuildCityMode::Browse && snapshot.selected_building_type >= 1 &&
           snapshot.selected_building_type <= snapshot.max_unlocked_building_type &&
           snapshot.selected_building_type <= 4)
        {
            pulse_building_type = snapshot.selected_building_type;
        }
        else if(snapshot.mode == BuildCityMode::Placement && snapshot.pending_building_type >= 1 &&
                snapshot.pending_building_type <= 4)
        {
            pulse_building_type = snapshot.pending_building_type;
        }
    }

    // Saved towers: m.a places each strip frame at boardX+5 and uses a common
    // baseline boardY+15. Keeping the baseline (instead of centering in a cell)
    // is important because the four tower families have different heights.
    for(int index = 0; index < 25; ++index)
    {
        const CityTileSave& tile = save.city_tiles[index];
        if(tile.type < 1 || tile.type > 4)
        {
            continue;
        }

        const int column = index % 5;
        const int row = index / 5;
        const int screen_left = grid_screen_left + 5 + column * grid_spacing;
        const int screen_baseline = grid_screen_top + 15 + row * grid_spacing;
        _show_composite(
                building_asset(tile.type, tile.roof, _theme),
                building_center_x(tile.type, screen_left),
                building_center_y(tile.type, screen_baseline));
    }

    if(pulse_building_type != 0)
    {
        // m.a(Graphics, boolean) shows the same continuous type-colored pulse
        // in browser and placement modes. Create every ring before recoloring
        // so all sectors reuse one Butano BPP4 OBJ palette bank.
        bn::optional<bn::sprite_palette_ptr> valid_lot_palette;
        const int required = pulse_building_type - 1;
        for(int index = 0; index < 25; ++index)
        {
            if(int(snapshot.placement_capabilities[index]) >= required)
            {
                const int column = index % 5;
                const int row = index / 5;
                _show_valid_lot_ring(
                        cell_screen_left + 7 + column * grid_spacing,
                        cell_screen_top + 7 + row * grid_spacing,
                        valid_lot_palette);
            }
        }

        if(valid_lot_palette)
        {
            const uint32_t rgb = build_city_valid_lot_rgb(pulse_building_type, _placement_flash_ms);
            const bn::color color(
                    int((rgb >> 16) & 0xFFu) >> 3,
                    int((rgb >> 8) & 0xFFu) >> 3,
                    int(rgb & 0xFFu) >> 3);
            valid_lot_palette->set_color(1, color);
        }
    }

    if(snapshot.mode == BuildCityMode::Browse)
    {
        int selected = 0;
        if(snapshot.selected_building_type >= 1 && snapshot.selected_building_type <= 4)
        {
            selected = snapshot.selected_building_type;

            // Source m.t highlights the cursor row even when that type is
            // still locked; locked rows simply omit the tower/outline art.
            if(build_city_selector_slot_active(_selector_flash_ms))
            {
                const int slot_left = selector_screen_left + 2;
                const int slot_top = selector_screen_top + 2 + (selected - 1) * 16;
                _show_composite(
                        generated::city_selector_active_slot,
                        centered_x(slot_left + 7),
                        centered_y(slot_top + 7));
            }
        }

        // Recovered four-slot browser: every unlocked tower icon uses strip
        // frame 3. Draw neighboring previews first so the selected outline can
        // never be covered by an adjacent tower sprite.
        for(int type = 1; type <= snapshot.max_unlocked_building_type && type <= 4; ++type)
        {
            if(type == selected)
            {
                continue;
            }
            int screen_left = selector_screen_left + 4;
            int screen_baseline = selector_screen_top + 15 + (type - 1) * 16;
            _show_composite(
                    building_asset(type, 3, _theme),
                    building_center_x(type, screen_left),
                    building_center_y(type, screen_baseline));
        }

        if(selected >= 1 && selected <= snapshot.max_unlocked_building_type && selected <= 4)
        {
            int preview_left = selector_screen_left + 4;
            int preview_baseline = selector_screen_top + 15 + (selected - 1) * 16;
            if(build_city_preview_raised(selected, snapshot.selected_building_type,
                                         snapshot.max_unlocked_building_type))
            {
                preview_left += 2;
                preview_baseline -= 2;
            }

            const int highlight_left = preview_left - 2;
            const int highlight_top = preview_baseline - 21;
            _show_composite(
                    lot_asset(selected - 1, _theme),
                    centered_x(highlight_left + 11),
                    centered_y(highlight_top + 11),
                    selected_preview_outline_z_order);
            _show_composite(
                    building_asset(selected, 3, _theme),
                    building_center_x(selected, preview_left),
                    building_center_y(selected, preview_baseline),
                    selected_preview_tower_z_order);
        }
        return;
    }

    if(snapshot.mode == BuildCityMode::Placement)
    {
        // Keep the demolition/discard cell visible throughout placement.  The
        // GBA presentation intentionally moves it 10px lower than the source
        // Java anchor so it lines up with the left selector column.
        if(_theme == GameTheme::Christmas)
        {
            show_sprite_item(_sprites, christmas::action_icon, 0,
                             discard_cell_center_x, discard_cell_center_y);
        }
        else
        {
            _show_composite(
                    generated::city_action_icon,
                    centered_x(discard_cell_center_x), centered_y(discard_cell_center_y));
        }
    }

    if(snapshot.cursor_column < 0)
    {
        int effect_frame = -1;
        if(snapshot.placement_committing && snapshot.placement_timer_ms >= 0)
        {
            effect_frame = build_city_discard_effect_frame(snapshot.placement_timer_ms);
        }

        // The pending building sits slightly up-right inside the bulldozer
        // cell. Once the shared replacement/discard effect has shown its first
        // two destruction frames, hide the tower sprite so the latter frames
        // match the source demolition pass.
        const bool show_pending_building = snapshot.pending_building_type >= 1 &&
                snapshot.pending_building_type <= 4 && (! snapshot.placement_committing || effect_frame >= 4);
        if(show_pending_building)
        {
            const int type = snapshot.pending_building_type;
            _show_composite(
                    building_asset(type, snapshot.pending_roof, _theme),
                    centered_x(discard_building_center_x),
                    centered_y(discard_building_center_y),
                    placement_pending_z_order);
        }

        if(effect_frame >= 0)
        {
            _show_composite(
                    city_effect_asset(effect_frame, true, _theme),
                    centered_x(discard_cell_center_x), centered_y(discard_cell_center_y),
                    discard_effect_z_order);
        }
        return;
    }

    const int cell_center_x = cell_screen_left + 7 + snapshot.cursor_column * grid_spacing;
    const int cell_center_y = cell_screen_top + 7 + snapshot.cursor_row * grid_spacing;
    if(snapshot.placement_transition_ms == 0 && ! snapshot.placement_committing)
    {
        // Resource 28 frame 4 is the placement square. Its visible 14x14
        // pixels begin at logical source (0, 9) in a 23x23 canvas.
        const int cursor_canvas_left = cell_screen_left + snapshot.cursor_column * grid_spacing;
        const int cursor_canvas_top = cell_screen_top - 9 + snapshot.cursor_row * grid_spacing;
        _show_composite(
                lot_asset(4, _theme),
                centered_x(cursor_canvas_left + 11),
                centered_y(cursor_canvas_top + 11));
    }

    const bool target_is_grid_cell = snapshot.cursor_column >= 0 && snapshot.cursor_row >= 0;
    const bool replacing_existing_tile = target_is_grid_cell &&
            save.city_tiles[snapshot.cursor_row * 5 + snapshot.cursor_column].type != 0;
    const bool show_pending_building = snapshot.pending_building_type >= 1 && snapshot.pending_building_type <= 4 &&
            (! snapshot.placement_committing || ! replacing_existing_tile);
    if(show_pending_building)
    {
        const int target_left = grid_screen_left + 5 + snapshot.cursor_column * grid_spacing;
        const int target_baseline = grid_screen_top + 15 + snapshot.cursor_row * grid_spacing;
        int screen_left = target_left;
        int screen_baseline = target_baseline;
        if(snapshot.placement_transition_ms > 0)
        {
            const int elapsed = 750 - snapshot.placement_transition_ms;
            const int start_left = selector_screen_left + 4;
            const int start_baseline = selector_screen_top + 15 + (snapshot.pending_building_type - 1) * 16;
            screen_left = start_left + (target_left - start_left) * elapsed / 750;
            screen_baseline = start_baseline + (target_baseline - start_baseline) * elapsed / 750;
        }
        _show_composite(
                building_asset(snapshot.pending_building_type, snapshot.pending_roof, _theme),
                building_center_x(snapshot.pending_building_type, screen_left),
                building_center_y(snapshot.pending_building_type, screen_baseline));
    }

    if(snapshot.placement_committing && snapshot.placement_timer_ms >= 0)
    {
        const int tile_index = snapshot.cursor_row * 5 + snapshot.cursor_column;
        const bool replacing = save.city_tiles[tile_index].type != 0;
        const int effect_frame = build_city_placement_effect_frame(replacing, snapshot.placement_timer_ms);
        if(effect_frame >= 0)
        {
            _show_composite(city_effect_asset(effect_frame, replacing, _theme), centered_x(cell_center_x), centered_y(cell_center_y));
        }
    }
}

void BuildCityScene::_show_progress_line(const BuildCitySnapshot& snapshot)
{
    const int current = snapshot.current_milestone_population;
    const int next = snapshot.next_milestone_population;
    if(next <= current)
    {
        return;
    }

    // Draw the Build City red progress underline one pixel lower and let the
    // fully-complete state span the full usable top-bar width.
    constexpr int full_bar_x = 1;
    constexpr int full_bar_width = 238;
    int width = (snapshot.total_population - current) * full_bar_width / (next - current);
    if(width < 1) { width = 1; }
    if(width > full_bar_width) { width = full_bar_width; }

    int x = full_bar_x;
    while(width >= 8)
    {
        if(_theme == GameTheme::Christmas)
        {
            show_sprite_item(_sprites, christmas::progress, 0, x + 4, 21);
        }
        else
        {
            _show_composite(generated::city_progress_segment, centered_x(x + 4), centered_y(17));
        }
        x += 8;
        width -= 8;
    }
    if(width > 0)
    {
        if(_theme == GameTheme::Christmas)
        {
            show_sprite_item(_sprites, christmas::progress, width, x + 4, 21);
        }
        else
        {
            _show_composite(*city_progress_tails[width - 1], centered_x(x + width / 2), centered_y(17));
        }
    }
}

void BuildCityScene::_show_status(const SaveData& save, const BuildCitySnapshot& snapshot)
{
    _show_progress_line(snapshot);

    // Existing side-edge strips close the 1px black top-bar frame at x=0/239.
    // The four indexed city backgrounds supply the top and bottom edges.
    _show_composite(generated::city_edge_top_left, centered_x(1), centered_y(-1));
    _show_composite(generated::city_edge_top_right, centered_x(238), centered_y(-1));
    _show_composite(generated::city_edge_bottom_left, centered_x(1), centered_y(148));
    _show_composite(generated::city_edge_bottom_right, centered_x(238), centered_y(148));

    // The taller Build City top bar keeps the source-sized milestone and people
    // sprites; shift the entire HUD row down so their full height is centered
    // inside the expanded bar instead of clipping against its old Y anchor.
    constexpr int status_row_y = 9;
    constexpr int population_icon_y = status_row_y - 3;
    constexpr int population_counter_y = status_row_y - 1;
    constexpr int comparison_row_y = status_row_y;
    const bool empty_milestone_badge = snapshot.total_population == 0;
    // The original badge graphic already contains a black top border. Align
    // that row with the background's y=0 border instead of drawing a 2px
    // stripe above the badge. Keep its digits and the other HUD items at y=9.
    constexpr int badge_row_y = status_row_y - 1;
    if(_theme == GameTheme::Christmas)
    {
        const bn::sprite_item& left = empty_milestone_badge ?
                christmas::milestone_badge_empty_left : christmas::milestone_badge_left;
        const bn::sprite_item& right = empty_milestone_badge ?
                christmas::milestone_badge_empty_right : christmas::milestone_badge_right;
        show_sprite_item(_sprites, left, 0, 17, badge_row_y);
        show_sprite_item(_sprites, right, 0, 41, badge_row_y);
    }
    else
    {
        _show_composite(
                empty_milestone_badge ? generated::city_milestone_badge_empty : generated::city_milestone_badge,
                centered_x(25), centered_y(badge_row_y));
    }

    if(! empty_milestone_badge)
    {
        int milestone = snapshot.milestone;
        if(milestone < 0) { milestone = 0; }
        if(milestone > 20) { milestone = 20; }
        int milestone_x = milestone >= 10 ? 23 : 26;
        if(milestone >= 10)
        {
            if(_theme == GameTheme::Christmas)
            {
                show_sprite_item(_sprites, christmas::white_digits, milestone / 10, milestone_x + 2, status_row_y + 1);
            }
            else
            {
                _show_composite(*white_digits[milestone / 10], centered_x(milestone_x), centered_y(status_row_y));
            }
            milestone_x += 5;
        }
        if(_theme == GameTheme::Christmas)
        {
            show_sprite_item(_sprites, christmas::white_digits, milestone % 10, milestone_x + 2, status_row_y + 1);
        }
        else
        {
            _show_composite(*white_digits[milestone % 10], centered_x(milestone_x), centered_y(status_row_y));
        }
        milestone_x += 5;
        if(_theme == GameTheme::Christmas)
        {
            // Santa resource 27 stores '/' at frame 12 of its 14-glyph strip.
            show_sprite_item(_sprites, christmas::white_digits, 12, milestone_x + 2, status_row_y + 1);
        }
        else
        {
            _show_composite(generated::hud_white_digit_f12, centered_x(milestone_x), centered_y(status_row_y));
        }
        milestone_x += 5;
        if(_theme == GameTheme::Christmas)
        {
            show_sprite_item(_sprites, christmas::white_digits, 2, milestone_x + 2, status_row_y + 1);
        }
        else
        {
            _show_composite(*white_digits[2], centered_x(milestone_x), centered_y(status_row_y));
        }
        milestone_x += 5;
        if(_theme == GameTheme::Christmas)
        {
            show_sprite_item(_sprites, christmas::white_digits, 0, milestone_x + 2, status_row_y + 1);
        }
        else
        {
            _show_composite(*white_digits[0], centered_x(milestone_x), centered_y(status_row_y));
        }
    }

    // With the lowered row anchor the original 16x16 Build City population
    // icon fits again. Shift the six-cell counter slightly right so the wider
    // sprite does not collide with the first digit panel.
    constexpr int population_icon_x = 58;
    if(_theme == GameTheme::Christmas)
    {
        show_sprite_item(_sprites, christmas::population_icon, 0, population_icon_x + 4, population_icon_y + 4);
    }
    else
    {
        _show_composite(generated::city_population_icon, centered_x(population_icon_x), centered_y(population_icon_y));
    }

    // Resource 22 is the six-cell population backdrop. The JAR uses state 0
    // for the five digit cells and state 3 for the terminal cap when no
    // digit-roll is active.
    constexpr int population_counter_x = 75;
    for(int cell = 0; cell < 6; ++cell)
    {
        const int state = _population_roll.panel_state_for_cell(cell);
        if(_theme == GameTheme::Christmas)
        {
            show_sprite_item(_sprites, christmas::status_panels, state,
                             population_counter_x + cell * 8, population_counter_y + 3);
        }
        else
        {
            _show_composite(*city_panel_states[state], centered_x(population_counter_x + cell * 8), centered_y(population_counter_y));
        }
    }

    int population = snapshot.total_population;
    if(population < 0) { population = 0; }
    if(population > 99999) { population = 99999; }
    const generated::UiCompositeAsset* const* population_digits =
            _population_roll.use_red_digits() ? red_digits : brown_digits;
    const int visible_population_digits = 5 - _population_roll.changed_cells();
    int divisor = 10000;
    for(int index = 0; index < 5; ++index)
    {
        const int digit = (population / divisor) % 10;
        if(index < visible_population_digits)
        {
            if(_theme == GameTheme::Christmas)
            {
                const bn::sprite_item& item = _population_roll.use_red_digits() ?
                        christmas::red_digits : christmas::yellow_digits;
                show_sprite_item(_sprites, item, digit, population_counter_x + index * 8 + 2, population_counter_y + 5);
            }
            else
            {
                _show_composite(*population_digits[digit], centered_x(population_counter_x + index * 8), centered_y(population_counter_y));
            }
        }
        divisor /= 10;
    }

    const bool placement = snapshot.mode == BuildCityMode::Placement;
    const bool active_placement = placement && snapshot.placement_transition_ms == 0;
    constexpr int status_icon_x = 176;
    constexpr int comparison_panel_left_x = 194;
    constexpr int comparison_panel_right_x = 221;
    // The comparison boxes are inset one pixel above the regular HUD row.
    // Move the active outlines, tower badges and numbers as a group so they
    // sit within the same 8px-high frames as the inactive background boxes.
    constexpr int comparison_panel_row_y = comparison_row_y - 1;
    if(_theme == GameTheme::Christmas)
    {
        show_sprite_item(_sprites, christmas::status_icons, active_placement ? 1 : 0,
                         status_icon_x + 1, comparison_row_y + 1);
    }
    else
    {
        _show_composite(
                active_placement ? generated::city_status_placement : generated::city_status_browse,
                centered_x(status_icon_x), centered_y(comparison_row_y));
    }

    if(active_placement && snapshot.pending_building_type >= 1 && snapshot.pending_building_type <= 4)
    {
        if(_theme == GameTheme::Christmas)
        {
            show_sprite_item(_sprites, christmas::comparison_panel_active, 0,
                             comparison_panel_left_x + 4, comparison_panel_row_y + 4);
            show_sprite_item(_sprites, christmas::type_badges, snapshot.pending_building_type - 1,
                             187, comparison_panel_row_y + 1);
            show_christmas_comparison_digits(_sprites, snapshot.pending_population, 204, comparison_panel_row_y);
        }
        else
        {
            _show_composite(generated::city_comparison_panel_active, centered_x(comparison_panel_left_x), centered_y(comparison_panel_row_y));
            _show_composite(
                    *city_type_badges[snapshot.pending_building_type - 1], centered_x(185), centered_y(comparison_panel_row_y));
            show_comparison_digits(_sprites, white_digits, snapshot.pending_population, 204, comparison_panel_row_y);
        }

        int existing_type = 0;
        if(snapshot.cursor_column >= 0 && snapshot.cursor_column < 5 &&
           snapshot.cursor_row >= 0 && snapshot.cursor_row < 5)
        {
            existing_type = save.city_tiles[snapshot.cursor_row * 5 + snapshot.cursor_column].type;
        }
        if(existing_type >= 1 && existing_type <= 4)
        {
            if(_theme == GameTheme::Christmas)
            {
                show_sprite_item(_sprites, christmas::comparison_panel_active, 0,
                                 comparison_panel_right_x + 4, comparison_panel_row_y + 4);
                show_sprite_item(_sprites, christmas::type_badges, existing_type - 1,
                                 214, comparison_panel_row_y + 1);
                show_christmas_comparison_digits(_sprites, snapshot.replacement_population, 231, comparison_panel_row_y);
            }
            else
            {
                _show_composite(generated::city_comparison_panel_active, centered_x(comparison_panel_right_x), centered_y(comparison_panel_row_y));
                _show_composite(*city_type_badges[existing_type - 1], centered_x(212), centered_y(comparison_panel_row_y));
                show_comparison_digits(_sprites, white_digits, snapshot.replacement_population, 231, comparison_panel_row_y);
            }
        }
    }

    // Original neighbor requirements contain an escaped newline. Split it for
    // both the selector browser (where they normally appear) and placement,
    // instead of displaying literal backslash+n or overflowing the bottom bar.
    const auto show_instruction = [this](bn::string_view instruction)
    {
        bn::string<96> first_line;
        bn::string<96> second_line;
        bool on_second_line = false;
        for(int index = 0; index < instruction.size(); ++index)
        {
            if(instruction[index] == '\\' && index + 1 < instruction.size() &&
               instruction[index + 1] == 'n')
            {
                on_second_line = true;
                ++index;
            }
            else if(on_second_line)
            {
                second_line.append(instruction[index]);
            }
            else
            {
                first_line.append(instruction[index]);
            }
        }
        // English neighbor requirements fit across the GBA screen (the
        // longest line is 217px with this font, inside the 240px viewport).
        // Keep the original two-line layout for longer localized messages.
        if(on_second_line && _language == 0)
        {
            first_line.append(' ');
            first_line.append(second_line);
            (void) _text_generator.generate_optional(0, 68, first_line, _sprites);
        }
        else if(on_second_line)
        {
            (void) _text_generator.generate_optional(0, 63, first_line, _sprites);
            (void) _text_generator.generate_optional(0, 73, second_line, _sprites);
        }
        else
        {
            (void) _text_generator.generate_optional(0, 68, first_line, _sprites);
        }
    };

    if(snapshot.mode == BuildCityMode::Browse)
    {
        if(snapshot.selected_building_type > snapshot.max_unlocked_building_type)
        {
            const bn::string<96> locked = flatten_city_instruction(
                    generated::localized_strings[_language][63], snapshot.selected_unlock_population);
            (void) _text_generator.generate_optional(0, 68, locked, _sprites);
        }
        else if(snapshot.selected_building_type <= snapshot.max_trophy_building_type)
        {
            const int type_index = snapshot.selected_building_type - 1;
            const bn::string<96> trophy = format_trophy_instruction(
                    generated::localized_strings[_language][62],
                    generated::localized_strings[_language][83 + type_index],
                    trophy_population_thresholds[type_index]);
            show_instruction(trophy);
        }
        else
        {
            show_instruction(generated::localized_strings[_language][99 + snapshot.selected_building_type - 1]);
        }
    }
    else if(snapshot.placement_transition_ms == 0)
    {
        if(snapshot.placement_committing && snapshot.cursor_column >= 0)
        {
            bn::string<96> population_result;
            if(snapshot.last_population_delta > 0)
            {
                population_result = flatten_city_instruction(
                        generated::localized_strings[_language][67], snapshot.last_population_delta);
            }
            else if(snapshot.last_population_delta < 0)
            {
                population_result = flatten_city_instruction(
                        generated::localized_strings[_language][69], -snapshot.last_population_delta);
            }
            else
            {
                population_result = generated::localized_strings[_language][68];
            }

            // The JAR shows this result throughout the three-second placement
            // commit animation. Keep short strings on one line; wrap longer
            // localized messages into the same two-line bottom panel.
            if(population_result.size() <= 46)
            {
                (void) _text_generator.generate_optional(0, 68, population_result, _sprites);
            }
            else
            {
                int split = 46;
                while(split > 0 && population_result[split] != ' ')
                {
                    --split;
                }
                if(split <= 0)
                {
                    split = population_result.size() / 2;
                }

                bn::string<96> first_line;
                bn::string<96> second_line;
                for(int index = 0; index < population_result.size(); ++index)
                {
                    if(index < split)
                    {
                        first_line.append(population_result[index]);
                    }
                    else if(index > split || population_result[index] != ' ')
                    {
                        second_line.append(population_result[index]);
                    }
                }
                (void) _text_generator.generate_optional(0, 63, first_line, _sprites);
                (void) _text_generator.generate_optional(0, 73, second_line, _sprites);
            }
        }
        else
        {
            const char* instruction = snapshot.cursor_column < 0 ?
                    generated::localized_strings[_language][64] :
                    generated::localized_strings[_language][snapshot.placement_valid ? 65 : 66];
            show_instruction(instruction);
        }
    }

}

void BuildCityScene::_show_event_modal(const BuildCityEvent& event)
{
    const int modal_index = event.localization_index - generated::city_modal_min_string_index;
    if(modal_index < 0 || modal_index >= generated::city_modal_string_count)
    {
        return;
    }

    const int line_count = generated::city_modal_line_counts[_language][modal_index];
    const bool city_level_event = event.id >= 24 && event.id <= 32;
    const int backdrop_rows = city_level_event ? 6 : (line_count <= 2 ? 4 : (line_count >= 6 ? 6 : 5));
    _show_modal_backdrop(8, 32, 7, backdrop_rows);
    int y = city_level_event ? -31 : -((line_count - 1) * modal_line_spacing) / 2;
    for(int line = 0; line < line_count; ++line)
    {
        const bn::string<128> formatted = format_event_line(
                generated::city_modal_lines[_language][modal_index][line], event, _language);
        (void) _text_generator.generate_optional(0, y, formatted, _sprites);
        y += modal_line_spacing;
    }
    if(city_level_event)
    {
        _show_composite(city_level_icon_asset(event.id - 24, _theme), 0, 18, -100);
    }
    _show_composite(generated::support_nav_f2, 0, centered_y(134), -100);
    _show_composite(generated::city_continue_arrow, centered_x(232), centered_y(152), -100);
}

void BuildCityScene::_rebuild(const SaveData& save)
{
    const BuildCitySnapshot snapshot = _city.snapshot();
    int city_theme = snapshot.max_unlocked_building_type - 1;
    if(city_theme < 0) { city_theme = 0; }
    if(city_theme > 3) { city_theme = 3; }
    if(! _background || city_theme != _rendered_city_theme)
    {
        // Drop the former theme before attempting the new BG tile/palette
        // allocation. If Butano has not reclaimed its VRAM yet, retry later.
        _background.reset();
        _rendered_city_theme = -1;
        if(_theme == GameTheme::Christmas)
        {
            switch(city_theme)
            {
            case 1:
                _background = bn::regular_bg_items::christmas_city_bg_theme_1.create_bg_optional(0, 0);
                break;
            case 2:
                _background = bn::regular_bg_items::christmas_city_bg_theme_2.create_bg_optional(0, 0);
                break;
            case 3:
                _background = bn::regular_bg_items::christmas_city_bg_theme_3.create_bg_optional(0, 0);
                break;
            default:
                _background = bn::regular_bg_items::christmas_city_bg_theme_0.create_bg_optional(0, 0);
                break;
            }
        }
        else
        {
            switch(city_theme)
            {
            case 1:
                _background = bn::regular_bg_items::city_bg_theme_1.create_bg_optional(0, 0);
                break;
            case 2:
                _background = bn::regular_bg_items::city_bg_theme_2.create_bg_optional(0, 0);
                break;
            case 3:
                _background = bn::regular_bg_items::city_bg_theme_3.create_bg_optional(0, 0);
                break;
            default:
                _background = bn::regular_bg_items::city_bg_theme_0.create_bg_optional(0, 0);
                break;
            }
        }
        if(! _background)
        {
            _sprites.clear();
            _has_snapshot = false;
            return;
        }
        _background->set_priority(3);
        _rendered_city_theme = city_theme;
    }
    _sprites.clear();
    // Prioritize UI and dialogue over the optional, dense placement rings and
    // 25 building composites. Z-order still determines visual layering.
    if(_events.has_event())
    {
        _show_event_modal(_events.current_event());
    }
    _show_status(save, snapshot);
    _show_city_tiles(save, snapshot);
    _last_snapshot = snapshot;
    _has_snapshot = true;
}
}
