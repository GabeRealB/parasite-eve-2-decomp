#include "common.h"

#include "actors/actor_444000.h"

#include "gameplay/1BC.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

/// Walk the yaw `field_7C8` toward `arg1` (clamped to +/-0x200) by at most 0x71
/// per call, turn model part 3 by it through `func_actor_444000_80132808`, and
/// refresh part 3, the root of the fifth escort's model and part 4.
void func_actor_444000_80133C58(Actor444000* task, s16 arg1)
{
    Actor444000Work* work = task->field_1C;
    s16              value;

    value = arg1;
    if (arg1 > 0x200) {
        value = 0x200;
    }
    if (arg1 < -0x200) {
        value = -0x200;
    }

    if (work->field_7C8 < value) {
        if (value - work->field_7C8 >= 0x72) {
            work->field_7C8 = work->field_7C8 + 0x71;
        } else {
            work->field_7C8 = value;
        }
    } else if (value < work->field_7C8) {
        if (abs(work->field_7C8 - value) >= 0x72) {
            work->field_7C8 = work->field_7C8 - 0x71;
        } else {
            work->field_7C8 = value;
        }
    }

    ((TmdObject*)task->extra)->coords[3].flg = 0;
    Gp_UpdateCoord(&((TmdObject*)task->extra)->coords[3]);
    func_actor_444000_80132808(&((TmdObject*)task->extra)->coords[3], work->field_7C8);
    ((TmdObject*)task->extra)->coords[3].flg = 0;
    Gp_UpdateCoord(&((TmdObject*)task->extra)->coords[3]);
    ((TmdObject*)work->field_ECC[4]->task->extra)->coords[0].flg = 0;
    Gp_UpdateCoord(&((TmdObject*)work->field_ECC[4]->task->extra)->coords[0]);
    ((TmdObject*)task->extra)->coords[4].flg = 0;
    Gp_UpdateCoord(&((TmdObject*)task->extra)->coords[4]);
}

/// Walk the pitch `field_F00` toward `arg1` (clamped to 0..0x500) by at most
/// 0x10 per call, then pitch model parts 3 and 4 about x: part 3 to half of
/// it, part 4 against it, each net of the pitch it already has.
void func_actor_444000_80133DE4(Actor444000* task, s16 arg1)
{
    Actor444000Work* work = task->field_1C;
    s16              value;
    s16              pitch4;
    s16              pitch3;

    value = arg1;
    if (arg1 > 0x500) {
        value = 0x500;
    }
    if (arg1 < 0) {
        value = 0;
    }

    if (work->field_F00 < value) {
        if (value - work->field_F00 >= 0x11) {
            work->field_F00 = work->field_F00 + 0x10;
        } else {
            work->field_F00 = value;
        }
    } else if (value < work->field_F00) {
        if (abs(work->field_F00 - value) >= 0x11) {
            work->field_F00 = work->field_F00 - 0x10;
        } else {
            work->field_F00 = value;
        }
    }

    pitch4 = -ratan2(((TmdObject*)task->extra)->coords[4].coord.m[1][2],
                     ((TmdObject*)task->extra)->coords[4].coord.m[2][2]);
    pitch3 = -ratan2(((TmdObject*)task->extra)->coords[3].coord.m[1][2],
                     ((TmdObject*)task->extra)->coords[3].coord.m[2][2]);

    Gfx_RotMatrixX(&((TmdObject*)task->extra)->coords[3].coord, work->field_F00 / 2 - pitch3, 0);
    ((TmdObject*)task->extra)->coords[3].flg = 0;
    Gfx_RotMatrixX(&((TmdObject*)task->extra)->coords[4].coord, -work->field_F00 - pitch4, 0);
    ((TmdObject*)task->extra)->coords[4].flg = 0;
}

/// Seed the blend: `field_7BE` to 0x30 and `field_7C0` to 0x800. Slots 1 and up
/// of the first animation context of each of the three pairs take that rate,
/// and the same slots of the second context are reset to animation
/// `field_7BC`.
void func_actor_444000_80133F64(Actor444000* task)
{
    Actor444000Work* work;
    s32              i;

    work            = task->field_1C;
    work->field_7BE = 0x30;
    work->field_7C0 = 0x800;
    i               = 1;
    do {
        work->slots0[i].rate = work->field_7BE;
        Gp_AnimResetSlot(&work->anim1, i, work->field_7BC);
        i++;
    } while (i < 8);
    i = 1;
    do {
        work->slots2[i].rate = work->field_7BE;
        Gp_AnimResetSlot(&work->anim3, i, work->field_7BC);
        i++;
    } while (i < 4);
    i = 1;
    do {
        work->slots4[i].rate = work->field_7BE;
        Gp_AnimResetSlot(&work->anim5, i, work->field_7BC);
        i++;
    } while (i < 4);
}
