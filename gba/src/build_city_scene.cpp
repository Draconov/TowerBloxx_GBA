#include "tb/build_city_scene.h"

#include "tb/scene_backdrop.h"

#include "bn_bg_palettes.h"
#include "bn_color.h"
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

const generated::UiCompositeAsset* const city_effects[6] = {
    &generated::city_effect_f0,
    &generated::city_effect_f1,
    &generated::city_effect_f2,
    &generated::city_effect_f3,
    &generated::city_effect_f4,
    &generated::city_effect_f5,
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
           left.last_population_delta != right.last_population_delta;
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
    _language = language;
    _frame_phase = 0;
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

    if(result.exit)
    {
        _stop();
        return result;
    }

    const BuildCitySnapshot snapshot = _city.snapshot();
    if(! _has_snapshot || snapshot_changed(snapshot, _last_snapshot) || city_result.save_dirty || result.construction_requested)
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

void BuildCityScene::accept_constructed_tower(uint8_t building_type, int population, uint8_t roof)
{
    _city.accept_constructed_tower(building_type, population, roof);
    _has_snapshot = false;
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

void BuildCityScene::_show_city_tiles(const SaveData& save, const BuildCitySnapshot& snapshot)
{
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
            _show_composite(
                    building_asset(type, 3),
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
    _show_composite(*lot_assets[lot_frame], centered_x(cell_center_x), centered_y(cell_center_y));

    if(snapshot.pending_building_type >= 1 && snapshot.pending_building_type <= 4)
    {
        const int screen_left = grid_screen_left + 5 + snapshot.cursor_column * grid_spacing;
        const int screen_baseline = grid_screen_top + 15 + snapshot.cursor_row * grid_spacing;
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

void BuildCityScene::_show_status(const BuildCitySnapshot& snapshot)
{
    // Population icon and five right-aligned brown digits. These positions are
    // the exact call-site values from m.a(): icon clip at (3,1), number helper
    // x=58, y=2, width=5, spacing=8, count=5.
    _show_composite(generated::city_status_icon_f0, centered_x(6), centered_y(5));

    int population = snapshot.total_population;
    if(population < 0) { population = 0; }
    if(population > 99999) { population = 99999; }
    int divisor = 10000;
    for(int index = 0; index < 5; ++index)
    {
        const int digit = (population / divisor) % 10;
        const int screen_x = 23 + index * 8;
        _show_composite(*brown_digits[digit], centered_x(screen_x), centered_y(5));
        divisor /= 10;
    }

    // The source keeps a small status glyph just before the two top-right
    // population boxes. Frame 2 is the neutral gray browse-state glyph.
    _show_composite(generated::city_status_icon_f2, centered_x(183), centered_y(5));

    // Resource 22 supplies the source status-panel texture. Keep it in the top
    // HUD rather than replacing the panel with plain text as the old port did.
    _show_composite(generated::city_panel_f0, centered_x(202), centered_y(6));
    _show_composite(generated::city_panel_f1, centered_x(226), centered_y(6));

    // The original screen reserves its bottom 23px for one contextual line.
    // Do not put a giant "Build City" title or town label over the playfield.
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
    else
    {
        const char* instruction = snapshot.cursor_column < 0 ?
                generated::localized_strings[_language][64] :
                generated::localized_strings[_language][snapshot.placement_valid ? 65 : 66];
        _text_generator.generate(0, 68, instruction, _sprites);
    }
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
    _show_status(snapshot);
    _last_snapshot = snapshot;
    _has_snapshot = true;
}
}
