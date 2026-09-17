#include "common.h"

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
    coord = obj->field_8;
    work  = Mem_Calloc(0x58, 0);
    if (work == NULL) {
        Task_Kill(task);
        return;
    }
    task->idMap    = (TaskIdMap*)work;
    work->field_0  = GameFlag_GetNibble(0x49);
    work->field_16 = -1;
    work->field_17 = -1;
    obj->field_C  &= 0xFF7F;
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
    if (Game_Session->field_7 == 2) {
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

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory", func_dryfield_night_factory_8017F00C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory", func_dryfield_night_factory_8017F1DC);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory", D_dryfield_night_factory_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory", D_dryfield_night_factory_8017D5C4);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory", D_dryfield_night_factory_8017D5D0);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory", D_dryfield_night_factory_8017D5DC);
