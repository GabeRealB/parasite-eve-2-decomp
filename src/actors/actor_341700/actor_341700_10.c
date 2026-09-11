#include "common.h"

#include "main/gfx.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "actors/actor_341700.h"

void func_actor_341700_801651E0(Task* arg0);
void func_actor_341700_80168A14(Task* arg0);
void func_actor_341700_80168A48(Task* arg0);
void func_actor_341700_801697B8(Task* arg0);
void func_actor_341700_801697D4(Task* arg0);
void func_actor_341700_80169888(Task* arg0);
void func_actor_341700_8016999C(Task* arg0);
void func_actor_341700_80169AB0(Task* arg0);

/* Both are called with no argument: the caller's own `Task*` is already in
 * `$a0` at the `jal` and the callee reads it as its own `arg0`, so the target
 * has no register copy.  A real prototype would make GCC emit one, so these
 * stay unprototyped. */
s32 ActorsShared8016945c();
s32 ActorsShared8016974c();

/* This one, by contrast, is a normal prototyped call: the redundant
 * `move $a0, $s0` is deleted again after reload because `$a0` still holds the
 * caller's own `arg0`. */
s32 func_actor_341700_80168234(Task* arg0);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_10", func_actor_341700_801684A8);

/// The actor's six top-level state handlers.
extern TaskFuncTable6 D_actor_341700_80161E24;

void func_actor_341700_8016852C(Task* arg0)
{
    TaskFuncTable6 sp;

    sp = D_actor_341700_80161E24;
    sp.funcs[arg0->state](arg0);
}

void func_actor_341700_8016859C(Task* arg0)
{
    Actor341700Work* work                = (Actor341700Work*)arg0->idMap;
    void             (*states[2])(Task*) = {
        func_actor_341700_801697B8,
        func_actor_341700_801697D4,
    };

    states[(s16)work->field_420](arg0);
}
