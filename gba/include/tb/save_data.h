#ifndef TB_SAVE_DATA_H
#define TB_SAVE_DATA_H

#include <array>
#include <cstdint>

#include "tb/hall_of_fame.h"
#include "tb/quick_game.h"
#include "tb/theme.h"

namespace tb
{
inline constexpr uint32_t save_magic = 0x584C4254; // "TBLX" in little-endian memory.
inline constexpr uint16_t save_version = 4;
inline constexpr uint16_t legacy_save_version_v3 = 3;
inline constexpr uint16_t legacy_save_version_v2 = 2;
inline constexpr uint16_t legacy_save_version = 1;
inline constexpr uint8_t construction_instructions_seen_mask = 0x01;

struct LegacySaveDataV1
{
    uint32_t magic = save_magic;
    uint16_t version = legacy_save_version;
    uint8_t language = 0;
    uint8_t sound_enabled = 1;
    uint32_t quick_high_score = 0;
    uint32_t checksum = 0;
};

static_assert(sizeof(LegacySaveDataV1) == 16);

struct LegacySaveDataV2
{
    uint32_t magic = save_magic;
    uint16_t version = legacy_save_version_v2;
    uint8_t language = 0;
    uint8_t sound_enabled = 1;
    uint32_t quick_best_population = 0;
    uint32_t quick_best_height = 0;
    uint32_t quick_best_combo = 0;
    uint32_t checksum = 0;
};

static_assert(sizeof(LegacySaveDataV2) == 24);

struct CityTileSave
{
    int32_t population = 0;
    uint8_t type = 0;
    uint8_t roof = 0;
    uint8_t reserved0 = 0;
    uint8_t reserved1 = 0;
};

static_assert(sizeof(CityTileSave) == 8);

struct LegacySaveDataV3
{
    uint32_t magic = save_magic;
    uint16_t version = legacy_save_version_v3;
    uint8_t language = 0;
    uint8_t sound_enabled = 1;
    uint32_t quick_best_population = 0;
    uint32_t quick_best_height = 0;
    uint32_t quick_best_combo = 0;
    std::array<CityTileSave, 25> city_tiles{};
    std::array<uint8_t, 46> city_tutorial_flags{};
    std::array<uint8_t, 2> reserved{};
    uint32_t checksum = 0;
};
static_assert(sizeof(LegacySaveDataV3) == 272);

struct SaveData
{
    uint32_t magic = save_magic;
    uint16_t version = save_version;
    uint8_t language = 0;
    uint8_t sound_enabled = 1;
    uint32_t quick_best_population = 0;
    uint32_t quick_best_height = 0;
    uint32_t quick_best_combo = 0;
    std::array<CityTileSave, 25> city_tiles{};
    std::array<uint8_t, 46> city_tutorial_flags{};
    std::array<uint8_t, 2> reserved{};
    HallOfFameData hall_of_fame{};
    uint32_t checksum = 0;
};

struct QuickRecordFlags
{
    bool population = false;
    bool height = false;
    bool combo = false;

    [[nodiscard]] bool any() const
    {
        return population || height || combo;
    }
};

[[nodiscard]] SaveData make_default_save();
void finalize_save(SaveData& save);
[[nodiscard]] bool valid_save(const SaveData& save);
[[nodiscard]] bool valid_legacy_v3_save(const LegacySaveDataV3& save);
[[nodiscard]] bool valid_legacy_v2_save(const LegacySaveDataV2& save);
[[nodiscard]] bool valid_legacy_save(const LegacySaveDataV1& save);
[[nodiscard]] SaveData migrate_legacy_v3_save(const LegacySaveDataV3& legacy);
[[nodiscard]] SaveData migrate_legacy_v2_save(const LegacySaveDataV2& legacy);
[[nodiscard]] SaveData migrate_legacy_save(const LegacySaveDataV1& legacy);
[[nodiscard]] QuickRecordFlags apply_quick_result(SaveData& save, const QuickGameResult& result);
[[nodiscard]] bool construction_instructions_seen(const SaveData& save);
[[nodiscard]] bool mark_construction_instructions_seen(SaveData& save);
[[nodiscard]] GameTheme game_theme(const SaveData& save);
[[nodiscard]] bool set_game_theme(SaveData& save, GameTheme theme);
void reset_city_progress(SaveData& save);

#ifdef TB_HOST_TEST
void finalize_legacy_v3_save_for_test(LegacySaveDataV3& save);
void finalize_legacy_v2_save_for_test(LegacySaveDataV2& save);
void finalize_legacy_save_for_test(LegacySaveDataV1& save);
#endif
}

#endif
