#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/dryfield_night_factory.h"

extern void     Room_Util20(Task* task);
extern void     func_8004BFF8(s32 angle, MATRIX* matrix);
extern void     Room_Script16(Task* task);
extern TaskDesc D_dryfield_night_factory_80186DE0[];
extern TaskDesc D_dryfield_night_factory_80186E28[];

/// State 0 of the room's factory model: allocate the work block, seed it from
/// the progress nibble, point the model's coordinate at the seeded position
/// and the light/color matrices at the block's own, then pick the spawn table
/// for this session variant and hand the model to its own state machine.
///
/// The two spawn tables are passed straight to `Task_SpawnFromTable` from each
/// arm rather than through a variable: the argument is then a bare symbol, so
/// the `lui`/`addiu` pair is built in `$a0` itself and `jump2` merges the two
/// arms' identical tails back into one call.
void func_dryfield_night_factory_8017D6F8(Task* task)
{
    NightFactoryWork* work;
    GsCOORDINATE2*    coord;
    TmdObject*        obj;

    obj   = (TmdObject*)task->extra;
    coord = obj->coords;
    work  = memCalloc(0x58, 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work     = (TaskIdMap*)work;
    work->field_0  = GameFlag_GetNibble(0x49);
    work->field_16 = -1;
    work->field_17 = -1;
    obj->flags    &= 0xFF7F;
    if (work->field_0 & 1) {
        work->field_10 = 0x4000000;
        func_8004BFF8(0x4000000, &coord->coord);
    }
    if (work->field_0 & 2) {
        work->field_C.value = 0xFDC60000;
    } else {
        work->field_C.value = 0;
    }
    coord->coord.t[0] = 0xE4C;
    coord->coord.t[1] = work->field_C.part.whole;
    coord->coord.t[2] = 0x1AAE;
    Room_Util20(task);
    func_dryfield_night_factory_8017D858(task, 1, 0);
    if (gGameSession->at4.loc.stage == 2) {
        Task_SpawnFromTable(D_dryfield_night_factory_80186E28, 7, 0, (s32)task);
    } else {
        Task_SpawnFromTable(D_dryfield_night_factory_80186DE0, 7, 0, (s32)task);
    }
    task->exitCallback  = Room_Script16;
    task->killCountdown = 0;
    task->state++;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory", func_dryfield_night_factory_8017D858);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory", func_dryfield_night_factory_8017DA54);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory", func_dryfield_night_factory_8017DDD4);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory", func_dryfield_night_factory_8017E13C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory", func_dryfield_night_factory_8017E480);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory", func_dryfield_night_factory_8017E7A4);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory", func_dryfield_night_factory_8017EBD4);

/// State 1 of the room's cutscene sequence: the leg that runs while the nibble
/// of game flag 0x4E is 1. Step 0 zeroes the angular velocity and plays the
/// strain sound for this session variant, step 1 winds that velocity down to
/// -0x300000 as it accumulates into the 16.16 angle, and step 2 winds the
/// velocity back up to 0x100000 until the angle returns to -0x3000000, which it
/// pins there. Either of the last two steps advances when the angle crosses
/// that mark, and any other step reports the handler as finished -- which the
/// dispatcher `func_dryfield_night_factory_8017FD5C` answers by dropping the
/// sequence back to the shared state 0.
///
/// Every path ends the same way: the model's rotation matrix is rebuilt with
/// the identity splatted word-wise, five stores for the nine entries that
/// `RotMatrixX` then overwrites with the angle's integer part, and the
/// coordinate's `flg` is cleared. The day factory carries the same body at
/// `func_dryfield_factory_8017F8F4`.
s32 func_dryfield_night_factory_8017F00C(Task* task)
{
    NightFactoryCutsceneWork* work  = (NightFactoryCutsceneWork*)task->work;
    GsCOORDINATE2*            coord = ((TmdObject*)task->extra)->coords;
    MATRIX*                   m;
    s32                       ret = 0;

    switch (work->step) {
        case 0:
            work->field_0 = 0;
            if (gGameSession->at4.loc.stage == 2) {
                Gp_EnqueueStageSnd6(0x5217000D, (s8)Gp_GetObjPan(coord),
                                    (s8)gpGetObjDepth(coord));
            } else {
                Gp_EnqueueStageSnd6(0x5317000D, (s8)Gp_GetObjPan(coord),
                                    (s8)gpGetObjDepth(coord));
            }
            work->step++;
            break;
        case 1:
            work->field_0 += -0x28000;
            if (work->field_0 < -0x300000) {
                work->field_0 = -0x300000;
            }
            work->field_4.value += work->field_0;
            if (work->field_4.value < -0x3000000) {
                work->step++;
            }
            break;
        case 2:
            work->field_0 += 0x40000;
            if (work->field_0 > 0x100000) {
                work->field_0 = 0x100000;
            }
            work->field_4.value += work->field_0;
            if (work->field_4.value >= -0x3000000) {
                work->field_4.value = -0x3000000;
                work->step++;
            }
            break;
        default:
            ret = 1;
            break;
    }

    m                            = &coord->coord;
    *(s32*)&coord->coord.m[0][0] = 0x1000;
    *(s32*)&m->m[0][2]           = 0;
    *(s32*)&m->m[1][1]           = 0x1000;
    *(s32*)&m->m[2][0]           = 0;
    m->m[2][2]                   = 0x1000;
    RotMatrixX(work->field_4.part.whole, m);
    coord->flg = 0;
    return ret;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory", func_dryfield_night_factory_8017F1DC);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory", D_dryfield_night_factory_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory", D_dryfield_night_factory_8017D5C4);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory", D_dryfield_night_factory_8017D5D0);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory", D_dryfield_night_factory_8017D5DC);
