#include "common.h"

#include "actors/actor_215100.h"
#include "actors/actors_shared_8014c874.h"
#include "actors/actors_shared_801366fc.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/D4.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/tmd.h"

/// Imports from the 0x80180000 overlay; no header names them yet.
void            func_8017DCAC(s32 arg0);
void            func_8017F95C(s32 arg0);
void            func_80180B34(s32 arg0);
extern TaskDesc D_801856B8;
extern s32      D_actor_215100_8014EBE0;
extern s32      D_actor_215100_8014D038;
extern s32      D_actor_215100_8014D03C;
extern s32      D_actor_215100_8014CF84;
extern s32      D_actor_215100_8014CFAC;
extern s32      D_actor_215100_8014D010;
extern s32      D_actor_215100_8014D024;
extern s32      D_actor_215100_8014D040;
extern s32      D_actor_215100_8014EB98;
extern s32      D_actor_215100_8014ED90;
extern s32      D_actor_215100_8014EE68;
extern s32      D_actor_215100_8014EFA0;
extern s32      D_actor_215100_8014F060;
extern s32      D_actor_215100_8014F138;
extern s32      D_actor_215100_8015E670;

void func_actor_215100_80149F2C(Task* task)
{
    s16 slot;

    switch (task->state) {
        case 0x0:
            if (D_actor_215100_8014D038 != 0) {
                taskKill(task);
                return;
            }
            if (task->spawnArg1 == 0) {
                func_800E8614((s32)&D_actor_215100_8014ED90, 1);
            } else {
                slot = 0x1C;
                if (D_actor_215100_8014D040 == 0) {
                    slot = 0x15;
                }
                Gp_StartCapSlot(slot, 0, 0);
                func_800E8614((s32)&D_actor_215100_8014EE68, 1);
            }
            task->state++;
            break;
        case 0x2:
            gGameSession->eventState = 1;
            if (task->spawnArg1 != 0) {
                if (Gp_GetCapEventKey() != 0) {
                    task->state = 5;
                } else {
                    task->state = 4;
                }
            } else {
                task->state = 0xA;
            }
            break;
        case 0x4:
            slot = 8;
            if (D_actor_215100_8014D040 == 0) {
                slot = 0x19;
            }
            Gp_StartCapSlot(slot, 0, 0);
            func_800E8614((s32)&D_actor_215100_8014EBE0, 1);
            taskKill(task);
            break;
        case 0x5:
            slot = 7;
            if (D_actor_215100_8014D040 == 0) {
                slot = 0x18;
            }
            Gp_StartCapSlot(slot, 0, 0);
            func_800E8614((s32)&D_actor_215100_8014EB98, 1);
            task->state++;
            break;
        case 0x6:
            if (gGameSession->eventState == 0) {
                task->state = 0xA;
            }
            break;
        case 0xA:
            Gp_StartCapSlot(9, 0, 0);
            task->state++;
            break;
        case 0xB:
            if (Gp_CapBusy() == 0) {
                func_80180B34(0);
                task->state++;
            }
            break;
        case 0xC:
            task->state = 0x14;
            break;
        case 0x14:
            Gp_StartCapSlot(0xA, 0, 0);
            task->state++;
            break;
        case 0x15:
            if (Gp_CapBusy() == 0) {
                func_8017F95C(0);
                task->state++;
            }
            break;
        case 0x16:
            task->state = 0x1E;
            break;
        case 0x1E:
            Gp_PlayerWeaponId(&D_actor_215100_8014CF84);
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)&D_actor_215100_8014CF84, 0);
            Gp_DispatchMsg((Task*)Gp_LookupSlot4(1), 0x7D3, (s32)&D_actor_215100_8014D010, 0);
            task->killCountdown = 0x1B;
            Gp_StartCapSlot(0xB, 0, 0);
            task->state++;
            break;
        case 0x1F:
            if (task->killCountdown >= -0x3E7) {
                task->killCountdown--;
            }
            if (task->killCountdown == 0) {
                Gp_DispatchMsg((Task*)Gp_LookupSlot4(1), 0x7D3, (s32)&D_actor_215100_8014D024, 0);
            }
            if (task->killCountdown == -0x16) {
                Gp_DispatchMsg((Task*)Gp_LookupSlot4(1), 0x7D3, (s32)&D_actor_215100_8014CFAC, 0);
            }
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 0x20:
            D_actor_215100_8015E670 = Gp_GetCapEventKey();
            func_8017DCAC(D_actor_215100_8015E670);
            task->state = 0x28;
            break;
        case 0x28:
            Gp_StartCapSlot(Gp_GetCapEventKey() + 0xB, 0, 0);
            func_800E8614((s32)&D_actor_215100_8014F138, 1);
            task->state++;
            break;
        case 0x2A:
            task->state = 0x32;
            break;
        case 0x32:
            D_actor_215100_8014D040++;
            Gp_StateC08.field_6 |= 2;
            if (D_actor_215100_8015E670 < 3) {
                func_800E8614((s32)&D_actor_215100_8014EFA0, 1);
            } else {
                func_800E8614((s32)&D_actor_215100_8014F060, 1);
            }
            D_actor_215100_8014D038 = 1;
            task->state++;
            break;
        case 0x1:
        case 0x29:
        case 0x33:
            if (gGameSession->eventState == 0) {
                task->state++;
            }
            break;
        case 0x34:
            gGameSession->flowFlags &= 0x7F;
            Task_SpawnFromTable(&D_801856B8, 0, D_actor_215100_8015E670 - 1, 0);
            D_actor_215100_8014D03C = 1;
            taskKill(task);
            break;
    }
}
