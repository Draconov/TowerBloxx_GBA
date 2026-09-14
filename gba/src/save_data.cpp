#include "tb/save_data.h"

#include <cstddef>

namespace tb
{
namespace
{
template<typename Data>
uint32_t checksum_for(const Data& data)
{
    constexpr uint32_t fnv_offset = 2166136261u;
    constexpr uint32_t fnv_prime = 16777619u;
    const auto* bytes = reinterpret_cast<const uint8_t*>(&data);
    uint32_t checksum = fnv_offset;
    for(std::size_t index = 0; index < offsetof(Data, checksum); ++index)
    {
        checksum ^= bytes[index];
        checksum *= fnv_prime;
    }
    return checksum;
}
}

SaveData make_default_save()
{
    SaveData save{};
    reset_hall_of_fame(save.hall_of_fame);
    finalize_save(save);
    return save;
}

void finalize_save(SaveData& save)
{
    save.checksum = checksum_for(save);
}

bool valid_save(const SaveData& save)
{
    return save.magic == save_magic &&
           save.version == save_version &&
           save.checksum == checksum_for(save);
}

bool valid_legacy_v3_save(const LegacySaveDataV3& save)
{
    return save.magic == save_magic && save.version == legacy_save_version_v3 && save.checksum == checksum_for(save);
}

bool valid_legacy_v2_save(const LegacySaveDataV2& save)
{
    return save.magic == save_magic &&
           save.version == legacy_save_version_v2 &&
           save.checksum == checksum_for(save);
}

bool valid_legacy_save(const LegacySaveDataV1& save)
{
    return save.magic == save_magic &&
           save.version == legacy_save_version &&
           save.checksum == checksum_for(save);
}

SaveData migrate_legacy_v3_save(const LegacySaveDataV3& legacy)
{
    SaveData save{};
    save.language = legacy.language; save.sound_enabled = legacy.sound_enabled; save.quick_best_population = legacy.quick_best_population; save.quick_best_height = legacy.quick_best_height; save.quick_best_combo = legacy.quick_best_combo; save.city_tiles = legacy.city_tiles; save.city_tutorial_flags = legacy.city_tutorial_flags; save.reserved = legacy.reserved; reset_hall_of_fame(save.hall_of_fame); finalize_save(save); return save;
}

SaveData migrate_legacy_v2_save(const LegacySaveDataV2& legacy)
{
    SaveData save{};
    save.language = legacy.language;
    save.sound_enabled = legacy.sound_enabled;
    save.quick_best_population = legacy.quick_best_population;
    save.quick_best_height = legacy.quick_best_height;
    save.quick_best_combo = legacy.quick_best_combo;
    reset_hall_of_fame(save.hall_of_fame);
    finalize_save(save);
    return save;
}

SaveData migrate_legacy_save(const LegacySaveDataV1& legacy)
{
    SaveData save{};
    save.language = legacy.language;
    save.sound_enabled = legacy.sound_enabled;
    save.quick_best_population = legacy.quick_high_score;
    reset_hall_of_fame(save.hall_of_fame);
    finalize_save(save);
    return save;
}

bool construction_instructions_seen(const SaveData& save)
{
    return (save.reserved[0] & construction_instructions_seen_mask) != 0;
}

bool mark_construction_instructions_seen(SaveData& save)
{
    if(construction_instructions_seen(save))
    {
        return false;
    }
    save.reserved[0] |= construction_instructions_seen_mask;
    return true;
}

void reset_city_progress(SaveData& save)
{
    for(CityTileSave& tile : save.city_tiles)
    {
        tile = {};
    }
    save.city_tutorial_flags.fill(0);
    forget_build_city_player(save.hall_of_fame);
}

QuickRecordFlags apply_quick_result(SaveData& save, const QuickGameResult& result)
{
    QuickRecordFlags flags;
    if(result.population > int(save.quick_best_population))
    {
        save.quick_best_population = uint32_t(result.population);
        flags.population = true;
    }
    if(result.height > int(save.quick_best_height))
    {
        save.quick_best_height = uint32_t(result.height);
        flags.height = true;
    }
    if(result.longest_combo > int(save.quick_best_combo))
    {
        save.quick_best_combo = uint32_t(result.longest_combo);
        flags.combo = true;
    }
    return flags;
}

#ifdef TB_HOST_TEST
void finalize_legacy_v3_save_for_test(LegacySaveDataV3& save)
{ save.checksum = checksum_for(save); }

void finalize_legacy_v2_save_for_test(LegacySaveDataV2& save)
{
    save.checksum = checksum_for(save);
}

void finalize_legacy_save_for_test(LegacySaveDataV1& save)
{
    save.checksum = checksum_for(save);
}
#endif
}
