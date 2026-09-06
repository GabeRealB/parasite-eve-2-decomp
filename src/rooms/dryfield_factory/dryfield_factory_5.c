#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

extern void Room_Util16(s32);
extern void Room_Util17(s32);

/// Cutscene driver for the factory room: silences both weapons, runs the cap
/// (cutscene) command in `Task::spawnArg1`, then waits for the cap to report
/// event key 3 before setting the two progress flags and starting the follow-up
/// cap slot. Any state past 4 restores the weapons and kills the task.

INCLUDE_ASM("rooms/nonmatchings/dryfield_factory/dryfield_factory_5", func_dryfield_factory_801804DC);

INCLUDE_ASM("rooms/nonmatchings/dryfield_factory/dryfield_factory_5", func_dryfield_factory_80180644);

INCLUDE_ASM("rooms/nonmatchings/dryfield_factory/dryfield_factory_5", func_dryfield_factory_801806B0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_factory/dryfield_factory_5", func_dryfield_factory_8018072C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_factory/dryfield_factory_5", func_dryfield_factory_80180784);

/// Shorter variant of the factory cutscene driver: silences both weapons, runs
/// the cap command in `Task::spawnArg1`, waits for the cap to finish, then on
/// event key 3 records progress flag 0x4A and kills the task after restoring
/// the weapons.
void func_dryfield_factory_801807DC(Task* task)
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
                Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F3, 0, 0);
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
            Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F3, 1, 0);
            Task_Kill(task);
            break;
    }
}
