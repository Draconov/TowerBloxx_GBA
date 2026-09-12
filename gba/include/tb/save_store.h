#ifndef TB_SAVE_STORE_H
#define TB_SAVE_STORE_H

#include "tb/save_data.h"

namespace tb
{
[[nodiscard]] SaveData load_save();
void store_save(SaveData save);
}

#endif
