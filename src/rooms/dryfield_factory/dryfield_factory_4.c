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

/// Cutscene driver for the factory room: silences both weapons, runs the cap
/// (cutscene) command in `Task::spawnArg1`, then waits for the cap to report
/// event key 3 before setting the two progress flags and starting the follow-up
/// cap slot. Any state past 4 restores the weapons and kills the task.

void func_dryfield_factory_8017FC18(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            Gp_MsgAllyWeapon(0);
            Gp_RunCapCmd(task->spawnArg1, 0);
            goto advance;
        case 1:
            if (GameFlag_GetNibble(0x48) <= 0) {
                if (Game_Session->field_7 == 2) {
                    Room_Util17(0);
                    SOFT_BARRIER();
                } else {
                    Room_Util17(0);
                }
                Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F3, 0, 0);
            }
            task->state++;
            /* fallthrough */
        case 2:
            if (Gp_CapBusy() != 0) {
                return;
            }
            /* fallthrough */
        case 3:
            if (Gp_GetCapEventKey() == 3) {
                GameFlag_SetNibble(0x48, 1);
                GameFlag_SetNibble(0x4A, 1);
                if (Game_Session->field_7 == 2) {
                    Room_Util17(1);
                    Room_Util16(1);
                    SOFT_BARRIER();
                } else {
                    Room_Util17(1);
                    Room_Util16(1);
                }
                Gp_StartCapSlot(task->spawnArg1, 1, 2);
            }
        advance:
            task->state++;
            return;
        case 4:
            if (Gp_CapBusy() != 0) {
                return;
            }
            /* fallthrough */
        default:
            Gp_MsgPlayerWeapon(1);
            Gp_MsgAllyWeapon(1);
            Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F3, 1, 0);
            Task_Kill(task);
            break;
    }
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_factory/dryfield_factory_4", func_dryfield_factory_8017FDDC);

INCLUDE_ASM("rooms/nonmatchings/dryfield_factory/dryfield_factory_4", func_dryfield_factory_8018001C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_factory/dryfield_factory_4", func_dryfield_factory_801802F0);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_factory/dryfield_factory_4", RoomsShared8017fc38Table);
