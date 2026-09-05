#include "common.h"

#include "main/gameflag.h"
#include "mapui/map_dryfield_full.h"

s32 func_map_dryfield_full_80179954(MapDryfieldFullRec* arg0, MapDryfieldFullOut* arg1)
{
    if ((arg0->field_0 == 0x1A) && (arg0->field_5 == 0)) {
        arg1->field_3 = GameFlag_GetNibble(0x9F) + 1;
    }
    return 1;
}

/// Package header word: the map id this overlay carries.
const s32 D_map_dryfield_full_80179950 = 0x11A;
