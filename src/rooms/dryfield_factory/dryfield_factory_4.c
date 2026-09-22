#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

extern void Room_Util16(s32);
extern void Room_Util17(s32);

extern GpGridParams D_dryfield_factory_80186C68;
extern GpGridParams D_dryfield_factory_80187BF0;
extern GpGridParams D_dryfield_factory_80187BF8;

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
                if (gGameSession->at4.loc.stage == 2) {
                    Room_Util17(0);
                    SOFT_BARRIER();
                } else {
                    Room_Util17(0);
                }
                Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F3, 0, 0);
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
                if (gGameSession->at4.loc.stage == 2) {
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
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F3, 1, 0);
            taskKill(task);
            break;
    }
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_factory/dryfield_factory_4", func_dryfield_factory_8017FDDC);

/// Restores two faces of the room's collision grid (their normals, corners and
/// face records) from a template, then slides the eight corners 2000 units
/// along x once game flag 0x47 is set: at once in state 0, which then kills the
/// task, or from state 1 when the flag turns positive later.
void func_dryfield_factory_8018001C(Task* task)
{
    GpGridParams* src = &D_dryfield_factory_80186C68;
    GpGridParams* geom;
    s32           i;

    if (gGameSession->at4.loc.stage == 2) {
        geom = &D_dryfield_factory_80187BF8;
    } else {
        geom = &D_dryfield_factory_80187BF0;
    }
    switch (task->state) {
        case 0:
            for (i = 0; i < 2; i++) {
                geom->field_4[i].vx         = src->field_4[i].vx;
                geom->field_4[i].vy         = src->field_4[i].vy;
                geom->field_4[i].vz         = src->field_4[i].vz;
                geom->field_8[i * 4 + 0].vx = src->field_8[i * 4 + 0].vx;
                geom->field_8[i * 4 + 0].vy = src->field_8[i * 4 + 0].vy;
                geom->field_8[i * 4 + 0].vz = src->field_8[i * 4 + 0].vz;
                geom->field_8[i * 4 + 1].vx = src->field_8[i * 4 + 1].vx;
                geom->field_8[i * 4 + 1].vy = src->field_8[i * 4 + 1].vy;
                geom->field_8[i * 4 + 1].vz = src->field_8[i * 4 + 1].vz;
                geom->field_8[i * 4 + 2].vx = src->field_8[i * 4 + 2].vx;
                geom->field_8[i * 4 + 2].vy = src->field_8[i * 4 + 2].vy;
                geom->field_8[i * 4 + 2].vz = src->field_8[i * 4 + 2].vz;
                geom->field_8[i * 4 + 3].vx = src->field_8[i * 4 + 3].vx;
                geom->field_8[i * 4 + 3].vy = src->field_8[i * 4 + 3].vy;
                geom->field_8[i * 4 + 3].vz = src->field_8[i * 4 + 3].vz;
                geom->field_C[i]            = src->field_C[i];
            }
            if (GameFlag_GetNibble(0x47) != 0) {
                for (i = 0; i < 8; i++) {
                    geom->field_8[i].vx += 2000;
                }
                taskKill(task);
                return;
            }
            task->state++;
            break;
        case 1:
            if (GameFlag_GetNibble(0x47) > 0) {
                for (i = 0; i < 8; i++) {
                    geom->field_8[i].vx += 2000;
                }
                task->state++;
            }
            break;
        default:
            taskKill(task);
            break;
    }
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_factory/dryfield_factory_4", func_dryfield_factory_801802F0);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_factory/dryfield_factory_4", RoomsShared8017fc38Table);
