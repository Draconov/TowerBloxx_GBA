#include "tb/build_city_scene.h"

#include "tb/scene_backdrop.h"
#include "tb/build_city_visuals.h"

#include "bn_bg_palettes.h"
#include "bn_color.h"
#include "bn_sprite_palette_ptr.h"
#include "bn_string.h"
#include "bn_string_view.h"
#include "bn_regular_bg_items_city_bg.h"

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

const generated::UiCompositeAsset* const city_progress_tails[7] = {
    &generated::city_progress_tail_f1,
    &generated::city_progress_tail_f2,
    &generated::city_progress_tail_f3,
    &generated::city_progress_tail_f4,
    &generated::city_progress_tail_f5,
    &generated::city_progress_tail_f6,
    &generated::city_progress_tail_f7,
};

const generated::UiCompositeAsset& building_asset(int type, int roof)
{
    if(type < 1) { type = 1; }
    if(type > 4) { type = 4; }
    if(roof < 0) { roof = 0; }
    if(roof > 3) { roof = 3; }
    return *building_assets[type - 1][roof];
}

int centered_x(int screen_x)
{
    return screen_x - screen_center_x;
}

int centered_y(int screen_y)
{
    return screen_y - screen_center_y;
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

bn::string<96> format_city_value(bn::string_view template_text, int value)
{
    bn::string<96> result;
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
        bn::vector<bn::sprite_ptr, 256>& sprites,
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
            sprites.push_back(part.item->create_sprite(
                    centered_x(digit_center_x + part.x), centered_y(screen_y + part.y)));
        }
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
    return left.mode != right.mode || left.total_population != right.total_population ||
           left.milestone != right.milestone || left.city_level != right.city_level ||
           left.max_unlocked_building_type != right.max_unlocked_building_type ||
           left.max_trophy_building_type != right.max_trophy_building_type ||
           left.selected_building_type != right.selected_building_type ||
           left.selected_unlock_population != right.selected_unlock_population ||
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
}

void BuildCityScene::start(const SaveData& save, int language)
{
    set_city_backdrop();
    _city.reset_from_save(save);
    _events.clear_runtime();
    _committed_progress = progress_state(_city.snapshot());
    _events.on_city_entered(_committed_progress, save);
    _placement_score_pending = false;
    _deferred_placement_score = 0;
    _language = language;
    _frame_phase = 0;
    _placement_flash_ms = 0;
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
                result.placement_committed = true;
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
    result.construction_requested = _city.construction_request().pending;

    const BuildCitySnapshot snapshot = _city.snapshot();
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
            result.placement_committed = true;
            result.committed_total_population = city_result.committed_total_population;
        }
    }

    if(snapshot.mode == BuildCityMode::Placement && ! snapshot.placement_committing)
    {
        _placement_flash_ms = (_placement_flash_ms + delta_ms) % 800;
    }
    else
    {
        _placement_flash_ms = 0;
    }

    if(result.exit)
    {
        _stop();
        return result;
    }

    const bool placement_animation = snapshot.mode == BuildCityMode::Placement && ! snapshot.placement_committing;
    if(! _has_snapshot || snapshot_changed(snapshot, _last_snapshot) || city_result.save_dirty ||
       result.construction_requested || _events.has_event() || placement_animation)
    {
        _rebuild(save);
    }
    return result;
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

void BuildCityScene::resume_presentation(const SaveData& save)
{
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

void BuildCityScene::_show_composite(const generated::UiCompositeAsset& asset, int x, int y)
{
    for(int index = 0; index < asset.part_count; ++index)
    {
        const generated::UiSpritePartAsset& part = asset.parts[index];
        _sprites.push_back(part.item->create_sprite(x + part.x, y + part.y));
    }
}

void BuildCityScene::_show_valid_lot_ring(
        int screen_x, int screen_y, bn::optional<bn::sprite_palette_ptr>& valid_lot_palette)
{
    const generated::UiCompositeAsset& asset = generated::city_valid_lot_ring;
    for(int index = 0; index < asset.part_count; ++index)
    {
        const generated::UiSpritePartAsset& part = asset.parts[index];
        bn::sprite_ptr sprite = part.item->create_sprite(
                centered_x(screen_x + part.x), centered_y(screen_y + part.y));
        if(! valid_lot_palette)
        {
            valid_lot_palette = sprite.palette();
        }
        _sprites.push_back(sprite);
    }
}

void BuildCityScene::_show_city_tiles(const SaveData& save, const BuildCitySnapshot& snapshot)
{
    if(snapshot.mode == BuildCityMode::Placement && snapshot.cursor_column >= 0 &&
       snapshot.placement_transition_ms == 0 && snapshot.pending_building_type >= 1 && ! _events.has_event())
    {
        // Create every ring while the source palette is still untouched so
        // Butano can reuse one BPP4 OBJ palette bank for all valid sectors.
        // Only after all ring sprites exist do we recolor that shared bank.
        bn::optional<bn::sprite_palette_ptr> valid_lot_palette;
        const int required = snapshot.pending_building_type - 1;
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
            const uint32_t rgb = build_city_valid_lot_rgb(snapshot.pending_building_type, _placement_flash_ms);
            const bn::color color(
                    int((rgb >> 16) & 0xFFu) >> 3,
                    int((rgb >> 8) & 0xFFu) >> 3,
                    int(rgb & 0xFFu) >> 3);
            valid_lot_palette->set_color(1, color);
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
                building_asset(tile.type, tile.roof),
                building_center_x(tile.type, screen_left),
                building_center_y(tile.type, screen_baseline));
    }

    if(snapshot.mode == BuildCityMode::Browse)
    {
        // Recovered four-slot browser: x=boardX-26; slot fill begins at x+2;
        // unlocked tower preview frame 3 uses x+4 and the y+15 baseline.
        for(int type = 1; type <= snapshot.max_unlocked_building_type && type <= 4; ++type)
        {
            const int screen_left = selector_screen_left + 4;
            const int screen_baseline = selector_screen_top + 15 + (type - 1) * 16;
            const int frame = type == snapshot.selected_building_type ? 1 : 3;
            _show_composite(
                    building_asset(type, frame),
                    building_center_x(type, screen_left),
                    building_center_y(type, screen_baseline));
        }
        return;
    }

    if(snapshot.cursor_column < 0)
    {
        // Resource 23 is the source demolish/action button shown beside the grid.
        _show_composite(generated::city_action_icon, centered_x(74), centered_y(103));
        return;
    }

    const int cell_center_x = cell_screen_left + 7 + snapshot.cursor_column * grid_spacing;
    const int cell_center_y = cell_screen_top + 7 + snapshot.cursor_row * grid_spacing;
    const int lot_frame = snapshot.placement_valid ? 4 : 3;
    if(snapshot.placement_transition_ms == 0)
    {
        _show_composite(*lot_assets[lot_frame], centered_x(cell_center_x), centered_y(cell_center_y));
    }

    if(snapshot.pending_building_type >= 1 && snapshot.pending_building_type <= 4)
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
                building_asset(snapshot.pending_building_type, snapshot.pending_roof),
                building_center_x(snapshot.pending_building_type, screen_left),
                building_center_y(snapshot.pending_building_type, screen_baseline));
    }

    if(snapshot.placement_committing && snapshot.placement_timer_ms >= 0)
    {
        int elapsed = 3000 - snapshot.placement_timer_ms;
        if(elapsed < 0)
        {
            elapsed = 0;
        }
        int effect_frame = (elapsed / 100) % 6;
        _show_composite(*city_effects[effect_frame], centered_x(cell_center_x), centered_y(cell_center_y));
    }
}

void BuildCityScene::_show_progress_line(const BuildCitySnapshot& snapshot)
{
    const int current = snapshot.current_milestone_population;
    const int next = snapshot.next_milestone_population;
    if(next <= current || snapshot.total_population <= current)
    {
        return;
    }

    int width = (snapshot.total_population - current) * 234 / (next - current);
    if(width < 0) { width = 0; }
    if(width > 234) { width = 234; }

    int x = 3;
    while(width >= 8)
    {
        _show_composite(generated::city_progress_segment, centered_x(x + 4), centered_y(12));
        x += 8;
        width -= 8;
    }
    if(width > 0)
    {
        _show_composite(*city_progress_tails[width - 1], centered_x(x + width / 2), centered_y(12));
    }
}

void BuildCityScene::_show_status(const SaveData& save, const BuildCitySnapshot& snapshot)
{
    _show_progress_line(snapshot);

    // Resource 20 is clipped into four 3x23 screen-edge strips.
    _show_composite(generated::city_edge_top_left, centered_x(1), centered_y(11));
    _show_composite(generated::city_edge_top_right, centered_x(238), centered_y(11));
    _show_composite(generated::city_edge_bottom_left, centered_x(1), centered_y(148));
    _show_composite(generated::city_edge_bottom_right, centered_x(238), centered_y(148));

    _show_composite(generated::city_population_icon, centered_x(7), centered_y(5));

    // Resource 22 is the six-cell population backdrop at x=19..44, not the
    // comparison HUD on the right.  The JAR uses state 0 for the five digit
    // cells and state 3 for the terminal cap when no digit-roll is active.
    for(int cell = 0; cell < 6; ++cell)
    {
        const int state = cell == 5 ? 3 : 0;
        _show_composite(*city_panel_states[state], centered_x(23 + cell * 8), centered_y(5));
    }

    int population = snapshot.total_population;
    if(population < 0) { population = 0; }
    if(population > 99999) { population = 99999; }
    int divisor = 10000;
    for(int index = 0; index < 5; ++index)
    {
        const int digit = (population / divisor) % 10;
        _show_composite(*brown_digits[digit], centered_x(23 + index * 8), centered_y(5));
        divisor /= 10;
    }

    const bool placement = snapshot.mode == BuildCityMode::Placement;
    const bool active_placement = placement && snapshot.placement_transition_ms == 0;
    _show_composite(
            active_placement ? generated::city_status_placement : generated::city_status_browse,
            centered_x(183), centered_y(5));

    if(active_placement && snapshot.pending_building_type >= 1 && snapshot.pending_building_type <= 4)
    {
        _show_composite(generated::city_comparison_panel_active, centered_x(201), centered_y(5));
        _show_composite(
                *city_type_badges[snapshot.pending_building_type - 1], centered_x(192), centered_y(5));
        show_comparison_digits(_sprites, white_digits, snapshot.pending_population, 211, 5);

        int existing_type = 0;
        if(snapshot.cursor_column >= 0 && snapshot.cursor_column < 5 &&
           snapshot.cursor_row >= 0 && snapshot.cursor_row < 5)
        {
            existing_type = save.city_tiles[snapshot.cursor_row * 5 + snapshot.cursor_column].type;
        }
        if(existing_type >= 1 && existing_type <= 4)
        {
            _show_composite(generated::city_comparison_panel_active, centered_x(225), centered_y(5));
            _show_composite(*city_type_badges[existing_type - 1], centered_x(216), centered_y(5));
            show_comparison_digits(_sprites, white_digits, snapshot.replacement_population, 235, 5);
        }
    }

    if(snapshot.mode == BuildCityMode::Browse)
    {
        if(snapshot.selected_building_type > snapshot.max_unlocked_building_type)
        {
            const bn::string<96> locked = format_city_value(
                    generated::localized_strings[_language][63], snapshot.selected_unlock_population);
            _text_generator.generate(0, 68, locked, _sprites);
        }
        else
        {
            _text_generator.generate(
                    0, 68,
                    generated::localized_strings[_language][99 + snapshot.selected_building_type - 1], _sprites);
        }
    }
    else if(snapshot.placement_transition_ms == 0)
    {
        const char* instruction = snapshot.cursor_column < 0 ?
                generated::localized_strings[_language][64] :
                generated::localized_strings[_language][snapshot.placement_valid ? 65 : 66];
        _text_generator.generate(0, 68, instruction, _sprites);
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
    int y = -((line_count - 1) * 10) / 2;
    for(int line = 0; line < line_count; ++line)
    {
        const bn::string<128> formatted = format_event_line(
                generated::city_modal_lines[_language][modal_index][line], event, _language);
        _text_generator.generate(0, y, formatted, _sprites);
        y += 10;
    }
    _show_composite(generated::city_continue_arrow, centered_x(232), centered_y(152));
}

void BuildCityScene::_rebuild(const SaveData& save)
{
    if(! _background)
    {
        _background = bn::regular_bg_items::city_bg.create_bg(0, 0);
        _background->set_priority(3);
    }
    _sprites.clear();
    const BuildCitySnapshot snapshot = _city.snapshot();
    _show_city_tiles(save, snapshot);
    _show_status(save, snapshot);
    if(_events.has_event())
    {
        _show_event_modal(_events.current_event());
    }
    _last_snapshot = snapshot;
    _has_snapshot = true;
}
}
