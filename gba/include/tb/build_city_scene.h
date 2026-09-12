#ifndef TB_BUILD_CITY_SCENE_H
#define TB_BUILD_CITY_SCENE_H

#include "bn_sprite_ptr.h"
#include "bn_sprite_text_generator.h"
#include "bn_vector.h"

#include "generated/tower_font.h"
#include "generated/tower_ui_assets.h"
#include "tb/build_city.h"

namespace tb
{
struct BuildCitySceneUpdateResult
{
    bool exit = false;
    bool save_dirty = false;
    bool construction_requested = false;
};

class BuildCityScene
{
public:
    explicit BuildCityScene(const SaveData& save);

    void start(const SaveData& save, int language);
    [[nodiscard]] BuildCitySceneUpdateResult update(const InputFrame& input, SaveData& save);
    [[nodiscard]] bool active() const;
    [[nodiscard]] BuildCityConstructionRequest construction_request() const;
    void clear_construction_request();
    void accept_constructed_tower(uint8_t building_type, int population, uint8_t roof);

private:
    void _stop();
    void _rebuild(const SaveData& save);
    void _show_composite(const generated::UiCompositeAsset& asset, int x, int y);
    void _show_city_tiles(const SaveData& save, const BuildCitySnapshot& snapshot);
    void _show_status(const BuildCitySnapshot& snapshot);

    BuildCity _city;
    bn::sprite_text_generator _text_generator;
    bn::vector<bn::sprite_ptr, 128> _sprites;
    int _language = 0;
    int _frame_phase = 0;
    BuildCitySnapshot _last_snapshot{};
    bool _has_snapshot = false;
    bool _active = false;
};
}

#endif
