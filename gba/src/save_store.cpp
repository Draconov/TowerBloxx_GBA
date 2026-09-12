#include "tb/save_store.h"

#include "bn_sram.h"

namespace tb
{
SaveData load_save()
{
    SaveData save{};
    bn::sram::read(save);
    if(valid_save(save))
    {
        return save;
    }

    LegacySaveDataV2 legacy_v2{};
    bn::sram::read(legacy_v2);
    if(valid_legacy_v2_save(legacy_v2))
    {
        save = migrate_legacy_v2_save(legacy_v2);
        bn::sram::write(save);
        return save;
    }

    LegacySaveDataV1 legacy{};
    bn::sram::read(legacy);
    if(valid_legacy_save(legacy))
    {
        save = migrate_legacy_save(legacy);
        bn::sram::write(save);
        return save;
    }

    save = make_default_save();
    bn::sram::write(save);
    return save;
}

void store_save(SaveData save)
{
    finalize_save(save);
    bn::sram::write(save);
}
}
