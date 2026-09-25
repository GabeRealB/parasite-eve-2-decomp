#include "common.h"

#include "actors/actor.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

/// The block above, published by `func_actor_110300_80131F9C` from the task's
/// `Task::work`.
extern Actor110300Work* D_actor_110300_8013A0A0;

/// The actor's own task, stored by the step-0 handler. The message handlers
/// drive the animation step driver and the model through it, and the helper
/// task's entry `func_actor_110300_80131FF8` parents its coordinate to one of
/// its model's nodes.
extern Task* D_actor_110300_8013A0A4;

/// The helper task `Task_SpawnFromTable` returns in the step-0 handler; the
/// visibility handler drives its model alongside the actor's, and the exit
/// callback kills it.
extern Task* D_actor_110300_8013A0A8;

/// Spawn descriptor table the step-0 handler spawns the helper task from:
/// index 0 is the actor's own dispatcher, index 1 the helper.
extern TaskDesc D_actor_110300_8013A06C[];

/// Animation source `func_800B3F84` seeds the work block's slots from.
extern u8 D_actor_110300_8013A084[];

/// Message table published as `Task::msgTable`: the 0x7D3 and 0x7D5 handlers
/// and a terminator.
extern GpMsgEntry D_actor_110300_8013A054[];

void func_actor_110300_80132020(GpEnemy* enemy, Task* task);
void func_actor_110300_80132088(Task* task);
void func_actor_110300_801320C4(Task* arg0);
void func_actor_110300_80132138(void);
void func_actor_110300_80132180(void);
void func_actor_110300_80132208(void);

/// `func_800B4114` is declared locally with a signed `arg2`; see the note in
/// `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Step 0 of the `func_actor_110300_80131F9C` dispatcher: allocate the work
/// block, publish it, and hand the model's animation context its slot array.
///
/// Every access to the block goes through `D_actor_110300_8013A0A0` rather
/// than the `memCalloc` result, which is why the pointer is reloaded at each
/// use instead of staying in a callee-saved register. The task's message table
/// becomes the one holding the animation-start and visibility handlers.
void func_actor_110300_80131E24(GpEnemy* enemy, Task* task)
{
    VECTOR         vec;
    void*          work;
    TmdObject*     obj;
    GsCOORDINATE2* coord;

    obj                     = task->extra;
    coord                   = obj->coords;
    work                    = memCalloc(sizeof(Actor110300Work), 0);
    D_actor_110300_8013A0A0 = work;
    task->work              = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback           = func_actor_110300_80132088;
    coord->sub                   = &gGfxViewCoord;
    enemy->field_4               = &coord->coord;
    enemy->node.state.b.flags    = 1;
    enemy->field_48              = 0;
    enemy->node.state.b.targeted = 0;
    obj->otOffset                = 0;
    coord->flg                   = 0;
    D_actor_110300_8013A0A4      = task;
    D_actor_110300_8013A0A8      = Task_SpawnFromTable(D_actor_110300_8013A06C, 1, 0, 0);
    func_800B3F84(&D_actor_110300_8013A0A0->anim, D_actor_110300_8013A084, obj,
                  D_actor_110300_8013A0A0->aux, D_actor_110300_8013A0A0->slots);
    D_actor_110300_8013A0A0->animId    = 1;
    D_actor_110300_8013A0A0->field_474 = 2;
    func_actor_110300_801320C4(task);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    D_actor_110300_8013A0A0->field_47A++;
    task->msgTable = D_actor_110300_8013A054;
    task->state++;
}

/// The actor's task entry: a two-state dispatcher whose handler table is built
/// on the stack. It publishes the task's work block in
/// `D_actor_110300_8013A0A0` before calling the handler, which is how the
/// overlay's other functions reach the block without the task.
void func_actor_110300_80131F9C(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_110300_80131E24,
        func_actor_110300_80132020,
    };

    D_actor_110300_8013A0A0 = task->work;
    fns[task->state](task->spawnArg2, task);
}

/// Entry of the helper task: parents the given task's model root to node 8 of
/// the actor's model.
void func_actor_110300_80131FF8(Task* arg0)
{
    GsCOORDINATE2* parent;
    GsCOORDINATE2* coord;

    parent     = ((TmdObject*)D_actor_110300_8013A0A4->extra)->coords;
    coord      = ((TmdObject*)arg0->extra)->coords;
    coord->flg = 0;
    coord->sub = parent + 8;
}

/// Step 1 of the `func_actor_110300_80131F9C` dispatcher: run the body the
/// actor's step selects, then refresh the model root as step 0 did by feeding
/// its world translation to `func_800D7A9C` (the light solve) against the
/// model object itself.
///
/// The body reaches the task through the second argument, so the incoming `$a1`
/// is copied into `$a0` (the first, unused, is the `GpEnemy*`): that copy is
/// what the first call's argument, and the `Task::extra` load feeding it, are
/// both read off.
void func_actor_110300_80132020(GpEnemy* enemy, Task* task)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    VECTOR         vec;

    obj   = task->extra;
    coord = obj->coords;
    func_actor_110300_801320C4(task);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
}

/// Exit callback the step-0 handler installs: kills the helper task, then
/// destroys the actor.
void func_actor_110300_80132088(Task* arg0)
{
    taskKill(D_actor_110300_8013A0A8);
    Gp_DestroyEnemy(arg0->spawnArg2, arg0);
}

/// Advances the animation per the work block's `field_474`: step 1 reseeds the
/// slots through `func_800B4114`, step 2 resets them outright, and either moves
/// on to step 3, which ticks them. The argument is never read.
void func_actor_110300_801320C4(Task* arg0)
{
    if (D_actor_110300_8013A0A0->field_474 == 1) {
        func_actor_110300_80132208();
        D_actor_110300_8013A0A0->field_474 = 3;
        return;
    }
    if (D_actor_110300_8013A0A0->field_474 == 2) {
        func_actor_110300_80132180();
        D_actor_110300_8013A0A0->field_474 = 3;
        return;
    }
    if (D_actor_110300_8013A0A0->field_474 == 3) {
        func_actor_110300_80132138();
    }
}

/// Ticks animation slots 1..0x13 of the work block's animation context.
void func_actor_110300_80132138(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickIndex(&D_actor_110300_8013A0A0->anim, i);
        i++;
    } while (i < 0x14);
}

/// Sets the rate of animation slots 1..0x13 to 1 and resets each of them to
/// the current animation id, then records that id as the one now playing.
void func_actor_110300_80132180(void)
{
    s32 i;

    i = 1;
    do {
        D_actor_110300_8013A0A0->slots[i].rate = 1;
        Gp_AnimResetSlot(&D_actor_110300_8013A0A0->anim, i, (s16)D_actor_110300_8013A0A0->animId);
        i++;
    } while (i < 0x14);
    D_actor_110300_8013A0A0->field_476 = D_actor_110300_8013A0A0->animId;
}

/// Reseeds animation slots 1..0x13 of the work block's animation context from
/// the current animation id through `func_800B4114` (arguments 0 and 8), then
/// records that id as the one now playing.
void func_actor_110300_80132208(void)
{
    s32 i;

    i = 1;
    do {
        func_800B4114(&D_actor_110300_8013A0A0->anim, i, (s16)D_actor_110300_8013A0A0->animId, 0, 8);
        i++;
    } while (i < 0x14);
    D_actor_110300_8013A0A0->field_476 = D_actor_110300_8013A0A0->animId;
}

/// Message 0x7D3 handler: starts animation `args->field_4`, rejecting anything
/// from 6 up, and leaves the actor in step 2 with `field_47A` cleared before
/// running the animation step driver.
///
/// The actor is read into a local between the first two stores: that puts the
/// global's `lui`/`lw` ahead of the `li 2` and leaves the `field_47A` clear
/// for the call's delay slot.
s32 func_actor_110300_80132280(Task* task, s32 arg1, GpAnimArg* args)
{
    Task* actor;

    if (args->field_4 < 6) {
        D_actor_110300_8013A0A0->animId    = args->field_4;
        actor                              = D_actor_110300_8013A0A4;
        D_actor_110300_8013A0A0->field_474 = 2;
        D_actor_110300_8013A0A0->field_47A = 0;
        func_actor_110300_801320C4(actor);
        return 0;
    }
    return -1;
}

/// Message 0x7D5 handler: shows or hides the actor's model and the helper
/// task's together. Bit 0 of `flags` clears both models' `TmdObject::flags`
/// (shown); without it both get 0x80 (hidden). Bit 1 additionally ORs in 0x4
/// on both.
s32 func_actor_110300_801322E0(Task* task, s32 arg1, s32 flags)
{
    TmdObject* self;
    TmdObject* other;

    self  = D_actor_110300_8013A0A4->extra;
    other = (TmdObject*)D_actor_110300_8013A0A8->extra;

    if (flags & 1) {
        self->flags  = 0;
        other->flags = 0;
    } else {
        self->flags  = 0x80;
        other->flags = 0x80;
    }

    if (flags & 2) {
        self->flags  |= 4;
        other->flags |= 4;
    }
    return 0;
}
