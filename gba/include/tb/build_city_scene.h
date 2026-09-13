#ifndef TB_BUILD_CITY_SCENE_H
#define TB_BUILD_CITY_SCENE_H

#include "bn_optional.h"
#include "bn_regular_bg_ptr.h"
#include "bn_sprite_ptr.h"
#include "bn_sprite_palette_ptr.h"
#include "bn_sprite_text_generator.h"
#include "bn_vector.h"

#include "generated/tower_font.h"
#include "generated/tower_ui_assets.h"
#include "tb/build_city.h"
#include "tb/build_city_events.h"

namespace tb
{
struct BuildCitySceneUpdateResult
{
    bool exit = false;
    bool save_dirty = false;
    bool construction_requested = false;
    bool placement_committed = false;
    int committed_total_population = 0;
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
    void accept_constructed_tower(uint8_t building_type, int population, uint8_t roof, const SaveData& save);
    void suspend_presentation();
    void resume_presentation(const SaveData& save);

private:
    void _stop();
    void _rebuild(const SaveData& save);
    void _show_composite(const generated::UiCompositeAsset& asset, int x, int y);
    void _show_city_tiles(const SaveData& save, const BuildCitySnapshot& snapshot);
    void _show_valid_lot_ring(int screen_x, int screen_y, int building_type);
    void _show_status(const SaveData& save, const BuildCitySnapshot& snapshot);
    void _show_progress_line(const BuildCitySnapshot& snapshot);
    void _show_event_modal(const BuildCityEvent& event);

    BuildCity _city;
    BuildCityEventController _events;
    BuildCityProgressState _committed_progress{};
    bn::optional<bn::regular_bg_ptr> _background;
    bn::sprite_text_generator _text_generator;
    bn::vector<bn::sprite_ptr, 256> _sprites;
    bn::optional<bn::sprite_palette_ptr> _valid_lot_palette;
    int _language = 0;
    int _frame_phase = 0;
    int _placement_flash_ms = 0;
    BuildCitySnapshot _last_snapshot{};
    bool _has_snapshot = false;
    bool _active = false;
    bool _placement_score_pending = false;
    int _deferred_placement_score = 0;
};
}

#endif
