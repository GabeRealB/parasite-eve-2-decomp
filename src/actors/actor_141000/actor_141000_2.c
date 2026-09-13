#include "common.h"

#include "actors/actor_141000.h"

#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/task.h"
#include "main/tmd.h"

/// The actor's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern TaskFuncTable3 D_actor_141000_80131E24;

/// State 0 of the handler table at 0x80131E3C: ramps the actor's Z scale by
/// 1/16 a frame and, on reaching 1.0, clamps it there and advances the state
/// index `field_C` the dispatcher at 0x80132D3C walks.
void func_actor_141000_80132E24(Task* arg0)
{
    Actor141000Work* work;
    u16              scale;

    work          = (Actor141000Work*)arg0->idMap;
    scale         = work->field_A + 0x100;
    work->field_A = scale;
    if ((s16)scale >= 0x1000) {
        work->field_A = 0x1000;
        work->field_C = work->field_C + 1;
    }
    func_actor_141000_80132FD0(((TmdObject*)arg0->extra)->field_8, 0);
    func_actor_141000_8013308C(((TmdObject*)arg0->extra)->field_8, (s16)work->field_A);
}

/// State 1 of the handler table at 0x80131E3C: holds for 0x1F frames, then
/// advances the state index `field_C` the dispatcher at 0x80132D3C walks.
void func_actor_141000_80132EB0(Task* arg0)
{
    Actor141000Work* work;
    u16              ticks;

    work          = (Actor141000Work*)arg0->idMap;
    ticks         = work->field_E + 1;
    work->field_E = ticks;
    if ((s16)ticks >= 0x1F) {
        work->field_C = work->field_C + 1;
    }
}

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
