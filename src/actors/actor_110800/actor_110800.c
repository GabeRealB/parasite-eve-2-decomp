#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

/// Per-actor work block for the `actor_110800` overlay, reached through the
/// global `func_actor_110800_801322A0` publishes.
///
/// `anim` sits at offset 0, so `&D_actor_110800_80139F10->anim` compiles to the
/// bare pointer load. `slots` follows it directly (0x28 apart, as
/// `Gp_AnimResetSlot` is handed `work + i * 0x28`), and the animation-id pair
/// sits at 0x476/0x478.
typedef struct Actor110800Work {
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot slots[0x14]; // the slot array `func_800B3F84` is handed
    /* 0x334 */ byte       aux[0x140];  // `GpAnimCtx.poses`, one 0x10-byte record per slot
    /* 0x474 */ s16        field_474;   // actor step: 1 and 2 select the body to run, which then advances it to 3
    /* 0x476 */ s16        field_476;   // copy of `animId`, kept for change detection
    /* 0x478 */ u16        animId;      // animation id the slots are seeded with
    /* 0x47A */ u16        field_47A;   // incremented by the step-0 handler, cleared by the animation-start handler
    /* 0x47C */ s16        field_47C;   // frame slot 19 or 16 `func_actor_110800_80131F9C` last cued a sound for, kept for change detection
} Actor110800Work;
STATIC_ASSERT_SIZEOF(Actor110800Work, 0x480);

/// Argument block of the 0x7D3 message: the animation to start, after a
/// 4-byte field the handler does not read.
typedef struct Actor110800AnimArgs {
    /* 0x0 */ byte pad_0[4];
    /* 0x4 */ s32  animId;
} Actor110800AnimArgs;

/// The block above, published by `func_actor_110800_801322A0` from the task's
/// `Task::work`.
extern Actor110800Work* D_actor_110800_80139F10;

/// The actor's own task, stored by the step-0 handler. The message handlers
/// drive the step dispatcher and the model through it, and
/// `func_actor_110800_801322FC` parents a coordinate to one of its model's
/// nodes.
extern GpActorWork* D_actor_110800_80139F14;

/// The helper task `Task_SpawnFromTable` returns in the step-0 handler; the
/// visibility handler drives its model alongside the actor's, and the exit
/// callback kills it.
extern Task* D_actor_110800_80139F18;

/// Spawn descriptor table the step-0 handler spawns the helper task from.
extern TaskDesc D_actor_110800_80139EDC[];

/// Animation source `func_800B3F84` seeds the work block's slots from.
extern u8 D_actor_110800_80139EF4[];

/// Message table published as `Task::msgTable`: the 0x7D3 and 0x7D5 handlers
/// below and a terminator.
extern GpMsgEntry D_actor_110800_80139EC4[];

/// `func_800B4114` is declared locally with a signed `arg2`; see the note in
/// `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

void func_actor_110800_8013232C(Task* task);
void func_actor_110800_80132368(Task* task);
void func_actor_110800_801323DC(void);
void func_actor_110800_80132424(void);
void func_actor_110800_801324AC(void);

/// Step 0 of the `func_actor_110800_801322A0` dispatcher: allocate the work
/// block, publish it, and hand the model's animation context its slot array.
///
/// Every access to the block goes through `D_actor_110800_80139F10` rather
/// than the `memCalloc` result, which is why the pointer is reloaded at each
/// use instead of staying in a callee-saved register. The task's message table
/// becomes the one holding the animation-start and visibility handlers.
void func_actor_110800_80131E24(GpEnemy* enemy, Task* task)
{
    VECTOR         vec;
    void*          work;
    TmdObject*     obj;
    GsCOORDINATE2* coord;

    obj                     = task->extra;
    coord                   = obj->coords;
    work                    = memCalloc(0x55C, 0);
    D_actor_110800_80139F10 = work;
    task->work              = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback      = func_actor_110800_8013232C;
    coord->sub              = &gGfxViewCoord;
    enemy->field_4          = &coord->coord;
    enemy->node.flags       = 1;
    enemy->field_48         = 0;
    enemy->node.targeted    = 0;
    obj->otOffset           = 0;
    coord->flg              = 0;
    D_actor_110800_80139F14 = (GpActorWork*)task;
    D_actor_110800_80139F18 = Task_SpawnFromTable(D_actor_110800_80139EDC, 1, 0, 0);
    func_800B3F84(&D_actor_110800_80139F10->anim, D_actor_110800_80139EF4, obj,
                  D_actor_110800_80139F10->aux, D_actor_110800_80139F10->slots);
    D_actor_110800_80139F10->animId    = 1;
    D_actor_110800_80139F10->field_474 = 2;
    func_actor_110800_80132368(task);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    D_actor_110800_80139F10->field_47A++;
    task->msgTable = D_actor_110800_80139EC4;
    task->state++;
}

/// Step 1 of the `func_actor_110800_801322A0` dispatcher, the walk/run
/// footstep cue:
/// run the body the actor's step selects, cue the sound the running
/// animation's frame table asks for, then refresh the model root as step 0 did
/// by feeding its world translation to `func_800D7A9C` (the light solve)
/// against the model object itself.
///
/// The two animation ids this actor plays carry a frame table each: id 4
/// watches slot 19 alone, id 5 watches slot 19 and then slot 16. An entry
/// latches the frame it fired for in `field_47C`, so a frame that is held over
/// several calls only cues once; the mask is the frame index of the slot's
/// halfword.
///
/// The body reaches the task through the second argument, so the incoming `$a1`
/// is copied into `$a0` (the first, unused, is the `GpEnemy*`), and the model
/// and its coordinate are read through that copy. `task->extra` is written
/// twice with the coordinate taken through the first read: that leaves cse's
/// load in a temporary and copies it into `obj`, which is the `move` between
/// the two loads the target has.
///
/// The switch reads `animId` signed. The field is unsigned, so the cast is
/// load-bearing: without it the halfword load is `lhu` where the target has
/// `lh`.
void func_actor_110800_80131F9C(GpEnemy* enemy, Task* task)
{
    GsCOORDINATE2* coord;
    TmdObject*     obj;
    VECTOR         vec;

    coord = ((TmdObject*)task->extra)->coords;
    obj   = (TmdObject*)task->extra;
    func_actor_110800_80132368(task);
    switch ((s16)D_actor_110800_80139F10->animId) {
        case 4:
            if ((D_actor_110800_80139F10->slots[19].curRec & 0x3FF) == 0xC8) {
                if (D_actor_110800_80139F10->field_47C != (D_actor_110800_80139F10->slots[19].curRec & 0x3FF)) {
                    SndEvt_EnqueueType6(0x510D0011, 0, 0);
                }
                D_actor_110800_80139F10->field_47C = D_actor_110800_80139F10->slots[19].curRec & 0x3FF;
            }
            if ((D_actor_110800_80139F10->slots[19].curRec & 0x3FF) == 0xCA) {
                if (D_actor_110800_80139F10->field_47C != (D_actor_110800_80139F10->slots[19].curRec & 0x3FF)) {
                    SndEvt_EnqueueType6(0x510D000D, 0, 0);
                }
                D_actor_110800_80139F10->field_47C = D_actor_110800_80139F10->slots[19].curRec & 0x3FF;
            }
            if ((D_actor_110800_80139F10->slots[19].curRec & 0x3FF) == 0xCD) {
                if (D_actor_110800_80139F10->field_47C != (D_actor_110800_80139F10->slots[19].curRec & 0x3FF)) {
                    SndEvt_EnqueueType6(0x510D000E, 0, 0);
                }
                D_actor_110800_80139F10->field_47C = D_actor_110800_80139F10->slots[19].curRec & 0x3FF;
            }
            break;
        case 5:
            if ((D_actor_110800_80139F10->slots[19].curRec & 0x3FF) == 0x115) {
                if (D_actor_110800_80139F10->field_47C != (D_actor_110800_80139F10->slots[19].curRec & 0x3FF)) {
                    SndEvt_EnqueueType6(0x510D000F, 0, 0);
                }
                D_actor_110800_80139F10->field_47C = D_actor_110800_80139F10->slots[19].curRec & 0x3FF;
            }
            if ((D_actor_110800_80139F10->slots[19].curRec & 0x3FF) == 0x11F) {
                if (D_actor_110800_80139F10->field_47C != (D_actor_110800_80139F10->slots[19].curRec & 0x3FF)) {
                    SndEvt_EnqueueType6(0x510D000F, 0, 0);
                }
                D_actor_110800_80139F10->field_47C = D_actor_110800_80139F10->slots[19].curRec & 0x3FF;
            }
            if ((D_actor_110800_80139F10->slots[16].curRec & 0x3FF) == 0xCE) {
                if (D_actor_110800_80139F10->field_47C != (D_actor_110800_80139F10->slots[16].curRec & 0x3FF)) {
                    SndEvt_EnqueueType6(0x510D0010, 0, 0);
                }
                D_actor_110800_80139F10->field_47C = D_actor_110800_80139F10->slots[16].curRec & 0x3FF;
            }
            if ((D_actor_110800_80139F10->slots[16].curRec & 0x3FF) == 0xD8) {
                if (D_actor_110800_80139F10->field_47C != (D_actor_110800_80139F10->slots[16].curRec & 0x3FF)) {
                    SndEvt_EnqueueType6(0x510D0010, 0, 0);
                }
                D_actor_110800_80139F10->field_47C = D_actor_110800_80139F10->slots[16].curRec & 0x3FF;
            }
            break;
    }
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
}

/// The actor's task entry: a two-state dispatcher whose handler table is built
/// on the stack. It publishes the task's work block in
/// `D_actor_110800_80139F10` before calling the handler, which is how the
/// overlay's other functions reach the block without the task.
void func_actor_110800_801322A0(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_110800_80131E24,
        func_actor_110800_80131F9C,
    };

    D_actor_110800_80139F10 = task->work;
    fns[task->state](task->spawnArg2, task);
}

/// Parents the given task's model root to node 8 of the actor's model, offset
/// -50 on x.
void func_actor_110800_801322FC(GpActorWork* arg0)
{
    GsCOORDINATE2* parent;
    GsCOORDINATE2* coord;

    parent            = D_actor_110800_80139F14->extra->coords;
    coord             = arg0->extra->coords;
    coord->flg        = 0;
    coord->coord.t[0] = -50;
    coord->sub        = parent + 8;
}

/// Exit callback the step-0 handler installs: kills the helper task, then
/// destroys the actor.
void func_actor_110800_8013232C(Task* arg0)
{
    taskKill(D_actor_110800_80139F18);
    Gp_DestroyEnemy(arg0->spawnArg2, arg0);
}

/// Advances the animation per the work block's `field_474`: step 1 reseeds the
/// slots through `func_800B4114`, step 2 resets them outright, and either moves
/// on to step 3, which ticks them. The argument is never read.
void func_actor_110800_80132368(Task* task)
{
    if (D_actor_110800_80139F10->field_474 == 1) {
        func_actor_110800_801324AC();
        D_actor_110800_80139F10->field_474 = 3;
        return;
    }
    if (D_actor_110800_80139F10->field_474 == 2) {
        func_actor_110800_80132424();
        D_actor_110800_80139F10->field_474 = 3;
        return;
    }
    if (D_actor_110800_80139F10->field_474 == 3) {
        func_actor_110800_801323DC();
    }
}

/// Ticks animation slots 1..0x13 of the work block's animation context.
void func_actor_110800_801323DC(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickIndex(&D_actor_110800_80139F10->anim, i);
        i++;
    } while (i < 0x14);
}

/// Sets the rate of animation slots 1..0x13 to 1 and resets each of them to
/// the current animation id, then records that id as the one now playing.
void func_actor_110800_80132424(void)
{
    s32 i;

    i = 1;
    do {
        D_actor_110800_80139F10->slots[i].rate = 1;
        Gp_AnimResetSlot(&D_actor_110800_80139F10->anim, i, (s16)D_actor_110800_80139F10->animId);
        i++;
    } while (i < 0x14);
    D_actor_110800_80139F10->field_476 = D_actor_110800_80139F10->animId;
}

/// Reseeds animation slots 1..0x13 of the work block's animation context from
/// the current animation id through `func_800B4114` (arguments 0 and 8), then
/// records that id as the one now playing.
void func_actor_110800_801324AC(void)
{
    s32 i;

    i = 1;
    do {
        func_800B4114(&D_actor_110800_80139F10->anim, i, (s16)D_actor_110800_80139F10->animId, 0, 8);
        i++;
    } while (i < 0x14);
    D_actor_110800_80139F10->field_476 = D_actor_110800_80139F10->animId;
}

/// Message 0x7D3 handler: starts animation `args->animId`, rejecting anything
/// from 6 up, and leaves the actor in step 2 with `field_47A` cleared before
/// running the animation step driver.
///
/// The actor is read into a local between the first two stores: that puts the
/// global's `lui`/`lw` ahead of the `li 2` and leaves the `field_47A` clear
/// for the call's delay slot.
s32 func_actor_110800_80132524(Task* task, s32 arg1, Actor110800AnimArgs* args)
{
    GpActorWork* actor;

    if (args->animId < 6) {
        D_actor_110800_80139F10->animId    = args->animId;
        actor                              = D_actor_110800_80139F14;
        D_actor_110800_80139F10->field_474 = 2;
        D_actor_110800_80139F10->field_47A = 0;
        func_actor_110800_80132368((Task*)actor);
        return 0;
    }
    return -1;
}

/// Message 0x7D5 handler: shows or hides the actor's model and the helper
/// task's together. Bit 0 of `flags` clears both models' `TmdObject::flags`
/// (shown); without it both get 0x80 (hidden). Bit 1 additionally ORs in 0x4
/// on both.
s32 func_actor_110800_80132584(Task* task, s32 arg1, s32 flags)
{
    TmdObject* self;
    TmdObject* other;

    self  = D_actor_110800_80139F14->extra;
    other = (TmdObject*)D_actor_110800_80139F18->extra;

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
