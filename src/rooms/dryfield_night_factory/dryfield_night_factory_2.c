#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

extern void func_dryfield_night_factory_80181620(s32);
extern void func_dryfield_night_factory_80181B38(s32);

/// Cutscene driver for the night factory room: silences both weapons, runs the
/// cap (cutscene) command in `Task::spawnArg1`, then waits for the cap to
/// report event key 3 before setting the two progress flags and starting the
/// follow-up cap slot. Any state past 4 restores the weapons and kills the task.
void func_dryfield_night_factory_8017F330(Task* task)
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
                    func_dryfield_night_factory_80181B38(0);
                    SOFT_BARRIER();
                } else {
                    func_dryfield_night_factory_80181B38(0);
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
                    func_dryfield_night_factory_80181B38(1);
                    func_dryfield_night_factory_80181620(1);
                    SOFT_BARRIER();
                } else {
                    func_dryfield_night_factory_80181B38(1);
                    func_dryfield_night_factory_80181620(1);
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

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_2", func_dryfield_night_factory_8017F4F4);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_2", func_dryfield_night_factory_8017F734);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_2", func_dryfield_night_factory_8017FA08);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_2", func_dryfield_night_factory_8017FB48);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_2", func_dryfield_night_factory_8017FB68);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_2", func_dryfield_night_factory_8017FBC8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_2", func_dryfield_night_factory_8017FBF4);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_2", func_dryfield_night_factory_8017FD5C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_2", func_dryfield_night_factory_8017FDC8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_2", func_dryfield_night_factory_8017FE44);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_2", func_dryfield_night_factory_8017FE9C);

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

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_2", func_dryfield_night_factory_80180038);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_2", func_dryfield_night_factory_8018007C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_2", func_dryfield_night_factory_80180164);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_2", func_dryfield_night_factory_801802C8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_2", func_dryfield_night_factory_80180438);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_2", func_dryfield_night_factory_80180574);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_2", func_dryfield_night_factory_8018076C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_2", func_dryfield_night_factory_8018080C);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_2", D_dryfield_night_factory_8017D638);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_2", func_dryfield_night_factory_80180814);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_2", func_dryfield_night_factory_80180914);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_2", func_dryfield_night_factory_80180980);

void func_dryfield_night_factory_801809EC(void)
{
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_2", func_dryfield_night_factory_801809F4);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_2", func_dryfield_night_factory_80180A4C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_2", func_dryfield_night_factory_80180BA4);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_2", D_dryfield_night_factory_8017D678);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_2", func_dryfield_night_factory_80180DE8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_2", func_dryfield_night_factory_801810D8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_2", func_dryfield_night_factory_80181538);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_2", func_dryfield_night_factory_80181620);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_2", func_dryfield_night_factory_8018169C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_2", func_dryfield_night_factory_80181718);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_2", func_dryfield_night_factory_80181768);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_2", func_dryfield_night_factory_80181778);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_2", func_dryfield_night_factory_8018182C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_2", func_dryfield_night_factory_80181938);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_2", func_dryfield_night_factory_8018196C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_2", func_dryfield_night_factory_801819BC);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_2", func_dryfield_night_factory_80181A24);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_2", func_dryfield_night_factory_80181AB8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_2", func_dryfield_night_factory_80181B38);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_2", func_dryfield_night_factory_80181BB4);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_2", func_dryfield_night_factory_80181C14);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_2", func_dryfield_night_factory_801825F0);
