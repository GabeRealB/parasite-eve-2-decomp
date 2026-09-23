#include "common.h"
#include "main/task.h"
#include "main/session.h"
#include "gameplay/gameplay.h"
#include "main/mem.h"
#include "main/display.h"
#include "rooms/shelter_b1_pod_access_tunnel.h"
extern TaskDesc D_shelter_b1_pod_access_tunnel_80182D2C;
extern TaskDesc D_801348D8;

void func_shelter_b1_pod_access_tunnel_8017E41C(s32 arg0)
{
    Task_SpawnFromTable(&D_shelter_b1_pod_access_tunnel_80182D2C, 0, arg0, 0);
}

void func_shelter_b1_pod_access_tunnel_8017E44C(Task* task)
{
    GpViewRec* view;
    VECTOR     vec;

    if (task->killCountdown < task->spawnArg1 && gGameSession->at4.loc.view == 0xB) {
        view   = Gp_GetStageView(&gGameSession->at4.loc);
        vec.vx = 0;
        vec.vy = 0x10;
        vec.vz = 0;
        ApplyTransposeMatrixLV(&view->mtx, &vec, &vec);
        view->mtx.t[0] += vec.vx;
        view->mtx.t[1] += vec.vy;
        view->mtx.t[2] += vec.vz;
        Gp_TrySpawnViewTask((s32)view);
        task->killCountdown++;
        return;
    }
    taskKill(task);
}

void func_shelter_b1_pod_access_tunnel_8017E52C(s32 arg0)
{
    Task_SpawnFromTable(&D_shelter_b1_pod_access_tunnel_80182D2C, 1, arg0, 0);
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_pod_access_tunnel/shelter_b1_pod_access_tunnel_3", func_shelter_b1_pod_access_tunnel_8017E55C);

void func_shelter_b1_pod_access_tunnel_8017E5B4(Task* task)
{
    ShelterB1PodAccessTunnelWork* work;

    if (gGameSession->at4.loc.view != 0xB) {
        taskKill(task);
        return;
    }
    work = memCalloc(sizeof(ShelterB1PodAccessTunnelWork), 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work   = work;
    work->speed  = 0xF00000 / task->spawnArg1;
    task->state += 1;
}

/// Append an 8-bit, ABR-0 `DR_TPAGE` for VRAM origin (`tpage`, `arg1`) to OT
/// slot 1023.
void func_shelter_b1_pod_access_tunnel_8017E66C(s32 tpage, s16 arg1)
{
    DR_TPAGE* p;
    s32       y;

    y              = arg1;
    p              = gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setDrawTPage(p, 1, 0, getTPage(1, 0, tpage & 0x3C0, y));
    addPrim(gGpuCurrentOt + 1023, p);
}

void func_shelter_b1_pod_access_tunnel_8017E704(void)
{
    Task_SpawnFromTable(&D_801348D8, 0, 0, 0);
}

void func_shelter_b1_pod_access_tunnel_8017E734(s32 arg0)
{
    Display_InitModeObj(Task_GetDescAt(&D_shelter_b1_pod_access_tunnel_80182D2C, 2U), arg0, 0, 0x100);
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_pod_access_tunnel/shelter_b1_pod_access_tunnel_3", func_shelter_b1_pod_access_tunnel_8017E778);
