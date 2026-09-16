#include "common.h"

#include <psyq/libgte.h>

#include "main/tmd.h"
#include "rooms/dryfield_breezeway.h"

/// 1 when `pos` is closer than 9 units to `target`: a real distance, since the
/// sum of the two squared component differences is square-rooted before the
/// comparison. Only `vx` and `vy` take part. `func_dryfield_breezeway_8017EB8C`
/// asks this of each point it generates while it looks for somewhere to put the
/// hotspot prompt, and leaves its loop on the first point this accepts, so the
/// answer marks the candidate that has converged onto the target.
s32 func_dryfield_breezeway_8017FAD0(DbwVec* target, DbwVec* pos)
{
    s16 dx = pos->vx - target->vx;
    s16 dy = pos->vy - target->vy;

    return SquareRoot0((dx * dx) + (dy * dy)) < 9;
}

/// Parks the room task's display object on the hotspot cursor: the position the
/// scan `func_dryfield_breezeway_8017EB8C` advanced to is carried into the
/// object's coordinate scaled by the depth it is placed at (`0x5DC` over 680),
/// and `flg` is cleared so the next coord-tree update rebuilds the world matrix
/// from the new translation. The scan calls this once, as it leaves its loop.
void func_dryfield_breezeway_8017FB30(Task* task, s16 arg1, s16 arg2)
{
    GsCOORDINATE2* coord = ((TmdObject*)task->extra)->field_8;

    coord->coord.t[2] = 0x5DC;
    coord->flg        = 0;
    coord->coord.t[0] = (arg1 * 0x5DC) / 680;
    coord->coord.t[1] = (arg2 * 0x5DC) / 680;
}

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
