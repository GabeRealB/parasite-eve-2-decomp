#include "common.h"

#include "actors/actor_141000.h"

#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/task.h"
#include "main/tmd.h"

/// The actor's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern TaskFuncTable3 D_actor_141000_80131E24;

INCLUDE_ASM("actors/nonmatchings/actor_141000/actor_141000_2", func_actor_141000_80132E24);

INCLUDE_ASM("actors/nonmatchings/actor_141000/actor_141000_2", func_actor_141000_80132EB0);

INCLUDE_ASM("actors/nonmatchings/actor_141000/actor_141000_2", func_actor_141000_80132EF4);

void func_actor_141000_80132FC8(void)
{
}

INCLUDE_ASM("actors/nonmatchings/actor_141000/actor_141000_2", func_actor_141000_80132FD0);

void func_actor_141000_8013308C(GsCOORDINATE2* arg0, s32 arg1)
{
    VECTOR scale;

    scale.vz = arg1;
    scale.vx = 0x1000;
    scale.vy = 0x1000;
    ScaleMatrix(&arg0->coord, &scale);
}

INCLUDE_ASM("actors/nonmatchings/actor_141000/actor_141000_2", func_actor_141000_801330C0);

void func_actor_141000_801331AC(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_141000_80131E24;
    sp.funcs[task->state](task);
}

/// Chains this actor's root coordinate under the spawner's, then hands the task
/// to it. Same attach shape as `ActorsShared80132450`, but the parent's part is
/// always its root coordinate and the actor is flagged for immediate unlink
/// (`killCountdown`) instead of surviving to a later state.
void func_actor_141000_80133204(Task* task)
{
    ((TmdObject*)task->extra)->field_8->sub = ((TmdObject*)((Task*)task->spawnArg2)->extra)->field_8;
    Task_Reparent((Task*)task->spawnArg2, task);
    task->killCountdown = 0x7FF;
    task->state        += 1;
}

void func_actor_141000_80133260(Actor141000* arg0)
{
    Actor141000Point sp10[24];
    s32              spD0;
    s32              spD4;

    func_actor_141000_801323F0(arg0, sp10, &spD0, &spD4);
    func_actor_141000_80131E94(arg0, sp10, spD0);
}

INCLUDE_ASM("actors/nonmatchings/actor_141000/actor_141000_2", func_actor_141000_801332A0);

INCLUDE_ASM("actors/nonmatchings/actor_141000/actor_141000_2", func_actor_141000_80133490);

INCLUDE_ASM("actors/nonmatchings/actor_141000/actor_141000_2", func_actor_141000_801335D4);

INCLUDE_ASM("actors/nonmatchings/actor_141000/actor_141000_2", func_actor_141000_801336DC);

INCLUDE_ASM("actors/nonmatchings/actor_141000/actor_141000_2", func_actor_141000_801338C0);

INCLUDE_ASM("actors/nonmatchings/actor_141000/actor_141000_2", func_actor_141000_8013392C);
