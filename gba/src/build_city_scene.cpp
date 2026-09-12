#include "tb/build_city_scene.h"

#include "bn_bg_palettes.h"
#include "bn_color.h"
#include "bn_string.h"

#include "generated/tower_localization.h"
#include "generated/tower_ui_assets.h"

namespace tb
{
namespace
{
constexpr int grid_left = -34;
constexpr int grid_top = -25;
constexpr int grid_spacing = 17;

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

const generated::UiCompositeAsset& building_asset(int type, int roof)
{
    if(type < 1) { type = 1; }
    if(type > 4) { type = 4; }
    if(roof < 0) { roof = 0; }
    if(roof > 3) { roof = 3; }
    return *building_assets[type - 1][roof];
}

bn::string<48> value_line(const char* label, int value)
{
    bn::string<48> result(label);
    result.append(": ");
    result.append(bn::to_string<8>(value));
    return result;
}

bool snapshot_changed(const BuildCitySnapshot& left, const BuildCitySnapshot& right)
{
    return left.mode != right.mode || left.total_population != right.total_population ||
           left.milestone != right.milestone || left.city_level != right.city_level ||
           left.max_unlocked_building_type != right.max_unlocked_building_type ||
           left.max_trophy_building_type != right.max_trophy_building_type ||
           left.selected_building_type != right.selected_building_type ||
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
    bn::bg_palettes::set_transparent_color(bn::color(0, 0, 0));
}

void BuildCityScene::start(const SaveData& save, int language)
{
    _city.reset_from_save(save);
    _language = language;
    _frame_phase = 0;
    _has_snapshot = false;
    _active = true;
    _rebuild(save);
}

BuildCitySceneUpdateResult BuildCityScene::update(const InputFrame& input, SaveData& save)
{
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
}

void BuildCityScene::accept_constructed_tower(uint8_t building_type, int population, uint8_t roof)
{
    _city.accept_constructed_tower(building_type, population, roof);
    _has_snapshot = false;
}

void BuildCityScene::_stop()
{
    _active = false;
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
    for(int index = 0; index < 25; ++index)
    {
        const int column = index % 5;
        const int row = index / 5;
        const int x = grid_left + column * grid_spacing;
        const int y = grid_top + row * grid_spacing;
        const CityTileSave& tile = save.city_tiles[index];
        if(tile.type >= 1 && tile.type <= 4)
        {
            _show_composite(building_asset(tile.type, tile.roof), x, y);
        }
    }

    if(snapshot.mode == BuildCityMode::Placement)
    {
        if(snapshot.cursor_column >= 0)
        {
            const int x = grid_left + snapshot.cursor_column * grid_spacing;
            const int y = grid_top + snapshot.cursor_row * grid_spacing;
            const int lot_frame = snapshot.placement_valid ? 4 : 3;
            _show_composite(*lot_assets[lot_frame], x, y);
            if(snapshot.pending_building_type >= 1 && snapshot.pending_building_type <= 4)
            {
                _show_composite(building_asset(snapshot.pending_building_type, snapshot.pending_roof), x, y - 3);
            }
        }
        else
        {
            _show_composite(*lot_assets[4], grid_left - 31, grid_top + 4 * grid_spacing);
        }
    }
}

void BuildCityScene::_show_status(const BuildCitySnapshot& snapshot)
{
    _text_generator.generate(0, -70, generated::localized_strings[_language][92], _sprites);
    _text_generator.generate(-70, -56, value_line(generated::localized_strings[_language][82], snapshot.total_population), _sprites);
    _text_generator.generate(62, -56, generated::localized_strings[_language][71 + snapshot.city_level], _sprites);

    if(snapshot.mode == BuildCityMode::Browse)
    {
        _text_generator.generate(0, 64,
                generated::localized_strings[_language][83 + snapshot.selected_building_type - 1], _sprites);
        _text_generator.generate(0, 76,
                generated::localized_strings[_language][99 + snapshot.selected_building_type - 1], _sprites);
        const BuildCityConstructionRequest request = _city.construction_request();
        if(request.pending)
        {
            _text_generator.generate(0, 44, value_line(generated::localized_strings[_language][80], request.target_height), _sprites);
        }
    }
    else
    {
        const char* instruction = snapshot.cursor_column < 0 ?
                generated::localized_strings[_language][64] :
                generated::localized_strings[_language][snapshot.placement_valid ? 65 : 66];
        _text_generator.generate(0, 66, instruction, _sprites);
    }
}

void BuildCityScene::_rebuild(const SaveData& save)
{
    _sprites.clear();
    const BuildCitySnapshot snapshot = _city.snapshot();
    _show_city_tiles(save, snapshot);
    _show_status(snapshot);
    _last_snapshot = snapshot;
    _has_snapshot = true;
}
}
