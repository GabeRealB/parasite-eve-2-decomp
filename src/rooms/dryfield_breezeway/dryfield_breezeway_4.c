#include "common.h"

#include <psyq/libgte.h>

#include "rooms/dryfield_breezeway.h"

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway_4", func_dryfield_breezeway_8017FAD0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway_4", func_dryfield_breezeway_8017FB30);

/// `GpMsgEntry` handler for message 0x13F1, the "can this key item be used
/// here?" query `Gp_UseKeyItemRow` sends to slot 7. `item` is the key item the
/// player highlighted; 0x11B is the only one the breezeway accepts, and the
/// answer is latched in the work block's `field_40` for
/// `func_dryfield_breezeway_8017FE08` to pick its next state from.
s32 func_dryfield_breezeway_8017FBC8(Task* task, s32 msgId, s32 item, s32 arg3)
{
    DbwEventWork* work = (DbwEventWork*)task->idMap;

    if (item == 0x11B) {
        work->field_40 = 1;
        return 1;
    }
    work->field_40 = 0;
    return 0;
}

s16 func_dryfield_breezeway_8017FBEC(s32 arg0, s32 arg1, s32 arg2, s32 arg3)
{
    SVECTOR vec;

    vec.vx = arg2 - arg0;
    vec.vy = arg3 - arg1;
    vec.vz = 0;
    VectorNormalSS(&vec, &vec);
    return ratan2(vec.vx, vec.vy);
}
