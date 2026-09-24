#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/dryfield_night_factory.h"

/// Runs the factory model task's current state, through a copy of its handler
/// table on the stack.
void func_dryfield_night_factory_8017FE44(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_factory_8017D5C4;
    sp.funcs[task->state](task);
}

/// Runs the cutscene task's current state, through a copy of its handler table
/// on the stack.
void func_dryfield_night_factory_8017FE9C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_factory_8017D5D0;
    sp.funcs[task->state](task);
}

/// Second cutscene driver for the night factory: silences both weapons, runs
/// the cap command in `Task::spawnArg1`, and once the cap reports event key 3
/// records progress flag 0x4A, restores the weapons and kills the task.
void func_dryfield_night_factory_8017FEF4(Task* task)
{
    s32 state = task->state;

    switch (state) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            Gp_MsgAllyWeapon(0);
            Gp_RunCapCmd(task->spawnArg1, 0);
            goto advance;
        case 1:
            if (GameFlag_GetNibble(0x4A) < 2) {
                Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F3, 0, 0);
            }
            task->state++;
            /* fallthrough */
        case 2:
            if (Gp_CapBusy() != 0) {
                return;
            }
        advance:
            task->state++;
            return;
        case 3:
            if (Gp_GetCapEventKey() == state) {
                GameFlag_SetNibble(0x4A, 2);
            }
            Gp_MsgPlayerWeapon(1);
            Gp_MsgAllyWeapon(1);
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F3, 1, 0);
            taskKill(task);
            break;
    }
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_4", D_dryfield_night_factory_8017D638);
