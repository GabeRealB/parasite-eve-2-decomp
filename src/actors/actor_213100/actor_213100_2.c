#include "common.h"
#include "actors/actor_213100.h"

#include "actors/actors_shared_801327b4.h"
#include "gameplay/1BC.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

/// The actor's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern TaskFuncTable3 D_actor_213100_80149E30;

/// Two-entry spawn table; index 1 is the child this init takes along, whose
/// own body is `func_actor_213100_80149FE4`.
extern TaskDesc D_actor_213100_801521A8;

/// Message dispatch table parked in `Task::msgTable` - message 0x7D3 is the
/// animation handler `func_actor_213100_8014A258`, which the init calls
/// directly with a preset of its own.
extern u8 D_actor_213100_801521C0[];

void func_actor_213100_8014A0B8(void)
{
}

void func_actor_213100_8014A0C0(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_213100_80149E30;
    sp.funcs[task->state](task);
}

/// Spawn/setup handler: allocates the 0x488-byte work block, parks it in
/// `Task::work` and seeds its two animation bytes to -1 - any `field_43D`
/// value then differs, so the first anim install always runs - then spawns the
/// child the actor carries with it. Either allocation failing exits the task
/// instead. The model child is flagged for the type-1 deferred kill, both
/// models are handed the work block's matrices, and the actor starts its
/// animation by calling the handler its message table routes 0x7D3 to with a
/// preset of `{ 0, 5, 0, 0, 0 }`, then installs that table and advances to
/// state 1.
void func_actor_213100_8014A118(Task* arg0)
{
    Actor213100Work*      work;
    Actor213100AnimPreset preset;
    TmdObject*            ext;
    Task*                 child;

    work = (Actor213100Work*)Mem_Calloc(0x488, 0);
    if (work == NULL) {
        Gp_EnemyTaskExit(arg0);
        return;
    }
    arg0->work      = (TaskIdMap*)work;
    work->field_43D = -1;
    work->field_43E = -1;
    work->field_484 = -1;
    child           = Task_SpawnFromTable(&D_actor_213100_801521A8, 1, 8, (s32)arg0);
    work->field_480 = child;
    if (child == NULL) {
        Gp_EnemyTaskExit(arg0);
        return;
    }
    func_actor_213100_8014A23C(arg0);
    ext             = arg0->extra;
    ext->flags     |= 0x80;
    ext             = work->field_480->extra;
    ext->flags     |= 0x80;
    preset.field_0  = 0;
    preset.field_4  = 5;
    preset.field_8  = 0;
    preset.field_C  = 0;
    preset.field_10 = 0;
    func_actor_213100_8014A258(arg0, 0, &preset, 0);
    arg0->msgTable     = D_actor_213100_801521C0;
    arg0->exitCallback = ActorsShared801327b4;
    arg0->state++;
}
