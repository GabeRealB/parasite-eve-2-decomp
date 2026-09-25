#include "common.h"

#include "actors/actors_shared_8013231c.h"

#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3E9C.h"
#include "gameplay/gameplay.h"

/// Work block the spawn state `func_actor_213100_8014A118` allocates
/// (`memCalloc(0x488)`) and parks in `Task::work` -- that slot is not a
/// `TaskIdMap` here.
///
/// It opens with the animation context the 0x7D3 handler
/// `func_actor_213100_8014A258` drives: the `GpAnimCtx` at the block's own
/// address, the 0x13 slots above it and the table at 0x30C, the three
/// arguments that handler hands `func_800B3F84`. `field_43C` latches once the
/// slots have been started, and gates the per-frame tick; `field_43E` and
/// `field_43D` hold the current bank index and animation id, seeded to -1 so
/// the first preset always installs. `light` / `color` are the matrices
/// `func_actor_213100_8014A23C` publishes on the model. `field_480` is the
/// child task the spawn state creates, whose model mirrors this one's
/// visibility; `field_484` is the countdown after which the tick frees the
/// model's buffers, -1 while idle.
typedef struct Actor213100Work {
    /* 0x000 */ GpAnimCtx    anim;
    /* 0x014 */ GpAnimSlot   slots[0x13];
    /* 0x30C */ byte         field_30C[0x130];
    /* 0x43C */ s8           field_43C;
    /* 0x43D */ s8           field_43D;
    /* 0x43E */ s8           field_43E;
    /* 0x43F */ byte         pad_43F[0x1];
    /* 0x440 */ MATRIX       light;
    /* 0x460 */ MATRIX       color;
    /* 0x480 */ struct Task* field_480;
    /* 0x484 */ s32          field_484;
} Actor213100Work;
STATIC_ASSERT_SIZEOF(Actor213100Work, 0x488);

/// Animation preset the 0x7D3 handler `func_actor_213100_8014A258` takes;
/// the spawn state builds one on its stack as `{ 0, 5, 0, 0, 0 }` and calls
/// the handler with it directly. `field_0` is the bank index into
/// `D_actor_213100_801521A4`, latched into `Actor213100Work::field_43E`;
/// `field_4` is the animation id, latched into `field_43D`; a nonzero
/// `field_8` installs the id through `func_800B4114`, which also takes
/// `field_C`, once the slots have been started. Nothing here reads
/// `field_10`; the size is the five words the spawn state stores.
typedef struct Actor213100AnimPreset {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
    /* 0x10 */ s32 field_10;
} Actor213100AnimPreset;
STATIC_ASSERT_SIZEOF(Actor213100AnimPreset, 0x14);

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// `gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Animation bank table the 0x7D3 handler indexes with the preset's
/// `field_0`.
extern void* D_actor_213100_801521A4[];

/// Spawn table the spawn state takes its child from; entry 1 is the child,
/// whose body is `func_actor_213100_80149FE4`.
extern TaskDesc D_actor_213100_801521A8;

/// Message table the spawn state installs at `Task::msgTable`: 0x7D3 is the
/// animation handler `func_actor_213100_8014A258`, 0x7D4 the placement
/// handler `func_actor_213100_8014A390` and 0x7D5 the display handler
/// `func_actor_213100_8014A40C`.
extern u8 D_actor_213100_801521C0[];

/// Per-view visibility table the tick indexes with the session's current
/// view: nonzero shows the actor and its child, zero hides both.
extern s8 D_actor_213100_801521E0[];

void func_actor_213100_8014A03C(Task* task);
void func_actor_213100_8014A0B8(Task* task);
void func_actor_213100_8014A118(Task* arg0);
void func_actor_213100_8014A21C(Task* arg0);
void func_actor_213100_8014A23C(Task* arg0);
s32  func_actor_213100_8014A258(Task* task, s32 arg1, Actor213100AnimPreset* msg, s32 arg3);

/// Per-frame tick: ticks the work block's animation slots once they have been
/// started, and while the model is shown samples the child part's
/// translation through `func_800EA1A8` and draws the ground shadow where it
/// hits. Once the view is ready, rebuilds that part's world matrix, hands its
/// translation to `func_800D7A9C`, and shows or hides this model and the
/// child's together from the per-view table. The work block's countdown then
/// frees the model's buffers as it reaches zero.
void func_actor_213100_80149E3C(Task* task)
{
    Actor213100Work* work;
    TmdObject*       extra;
    TmdObject*       child;
    VECTOR3          pos;
    s32              i;

    work  = (Actor213100Work*)task->work;
    extra = (TmdObject*)task->extra;
    if (work->field_43C != 0) {
        for (i = 1; i < 0x13; i++) {
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
    if (!(extra->flags & 0x80)) {
        if (func_800EA1A8((VECTOR3*)((TmdObject*)task->extra)->coords[1].workm.t, &pos) != 0) {
            Gp_DrawEffGroundQuad(&pos, 0x300, Gp_State1C->groundShade);
        }
    }
    if (gGameSession->viewReady != 0) {
        ((TmdObject*)task->extra)->coords[1].flg = 0;
        Gp_UpdateCoord(&((TmdObject*)task->extra)->coords[1]);
        func_800D7A9C(extra, (VECTOR*)((TmdObject*)task->extra)->coords[1].workm.t, 0, 3);
        child = (TmdObject*)work->field_480->extra;
        if (D_actor_213100_801521E0[gGameSession->at4.loc.view] != 0) {
            extra->flags &= ~0x80;
            child->flags &= ~0x80;
        } else {
            extra->flags |= 0x80;
            child->flags |= 0x80;
        }
    }
    if (work->field_484 >= 0) {
        if (work->field_484 == 0) {
            Tmd_FreeBuffers(extra);
        }
        work->field_484--;
    }
}

/// State table of the child the spawn state creates: attach to the parent,
/// idle, kill.
const TaskFuncTable3 D_actor_213100_80149E24 = {
    {
        func_actor_213100_8014A03C,
        func_actor_213100_8014A0B8,
        taskKill,
    },
};

/// Body of the child task: dispatches on its state through
/// `D_actor_213100_80149E24`.
void func_actor_213100_80149FE4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_213100_80149E24;
    sp.funcs[task->state](task);
}

/// State 0 of the child: chains the child's root coordinate under the
/// parent's skeleton part named by the spawn arguments (the parent task and
/// the part index it was spawned with), inherits the parent's light and colour
/// matrices, reparents the task so it runs with the parent, and advances to
/// the idle state.
void func_actor_213100_8014A03C(Task* task)
{
    Task*          parent;
    s32            part;
    TmdObject*     extra;
    TmdObject*     parentExtra;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* dest;

    parent          = (Task*)task->spawnArg2;
    part            = task->spawnArg1;
    extra           = (TmdObject*)task->extra;
    parentExtra     = (TmdObject*)parent->extra;
    coord           = extra->coords;
    dest            = &parentExtra->coords[part];
    coord->flg      = 0;
    coord->sub      = dest;
    extra->lightMtx = parentExtra->lightMtx;
    extra->colorMtx = parentExtra->colorMtx;
    Task_Reparent(parent, task);
    task->state += 1;
}

/// The child's idle state: does nothing.
void func_actor_213100_8014A0B8(Task* task)
{
}

/// The actor's three states: spawn, per-frame tick and teardown.
const TaskFuncTable3 D_actor_213100_80149E30 = {
    {
        func_actor_213100_8014A118,
        func_actor_213100_80149E3C,
        func_actor_213100_8014A21C,
    },
};

/// Body of the actor's task: dispatches on its state through
/// `D_actor_213100_80149E30`.
void func_actor_213100_8014A0C0(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_213100_80149E30;
    sp.funcs[task->state](task);
}

/// Spawn state: allocates the work block into `Task::work` and seeds its
/// animation bytes and countdown to -1, then spawns the child from entry 1 of
/// the spawn table, attached to part 8 of this actor's skeleton. Either
/// allocation failing exits the task instead. Both models start hidden and
/// take the work block's matrices; the actor starts its animation by calling
/// the 0x7D3 handler directly with the preset `{ 0, 5, 0, 0, 0 }`, then
/// installs its message table and exit callback and advances to the tick.
void func_actor_213100_8014A118(Task* arg0)
{
    Actor213100Work*      work;
    Actor213100AnimPreset preset;
    TmdObject*            ext;
    Task*                 child;

    work = (Actor213100Work*)memCalloc(0x488, 0);
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
    arg0->exitCallback = func_actor_213100_8014A21C;
    arg0->state++;
}

/// The actor's teardown state and its `Task::exitCallback`: hands the task to
/// `Gp_EnemyTaskExit`.
void func_actor_213100_8014A21C(Task* arg0)
{
    Gp_EnemyTaskExit(arg0);
}

/// Points the model's light and colour matrices at the work block's own pair.
void func_actor_213100_8014A23C(Task* arg0)
{
    TmdObject*       ext;
    Actor213100Work* work;

    ext           = arg0->extra;
    work          = (Actor213100Work*)arg0->work;
    ext->lightMtx = &work->light;
    ext->colorMtx = &work->color;
}

/// Message-0x7D3 handler, also called directly by the spawn state with the
/// initial preset. A changed bank index re-seeds the whole animation slot
/// array through `func_800B3F84` from the bank table and forgets the current
/// animation id. A changed animation id is then stored and installed on every
/// slot - through `func_800B4114` when the preset's `field_8` is set and the
/// slots have already been started, through `Gp_AnimResetSlot` otherwise -
/// after which every slot is ticked once and `field_43C` latches. An
/// unchanged id skips all of that. Returns 0.
s32 func_actor_213100_8014A258(Task* task, s32 arg1, Actor213100AnimPreset* msg, s32 arg3)
{
    Actor213100Work* work;
    TmdObject*       ext;
    s32              i;

    work = (Actor213100Work*)task->work;
    ext  = task->extra;
    if (msg->field_0 != work->field_43E) {
        work->field_43E = msg->field_0;
        work->field_43D = -1;
        func_800B3F84(&work->anim, D_actor_213100_801521A4[work->field_43E], ext, work->field_30C,
                      work->slots);
    }
    if (msg->field_4 != work->field_43D) {
        work->field_43D = msg->field_4;
        if (msg->field_8 != 0 && work->field_43C != 0) {
            for (i = 1; i < 0x13; i++) {
                func_800B4114(&work->anim, i, work->field_43D, 0, msg->field_C);
            }
        } else {
            for (i = 1; i < 0x13; i++) {
                Gp_AnimResetSlot(&work->anim, i, work->field_43D);
            }
        }
        for (i = 1; i < 0x13; i++) {
            Gp_AnimTickIndex(&work->anim, i);
        }
        work->field_43C = 1;
    }
    return 0;
}

/// Message-0x7D4 handler: places the actor at the message's arguments -
/// the translation goes straight into the root coordinate's local matrix, the
/// Euler angles into the coordinate's `rot` slot, from which the rotation is
/// rebuilt. Clearing `flg` has the world matrix recomputed. Returns 0.
s32 func_actor_213100_8014A390(Task* task, s32 arg1, GpPlaceArg* args)
{
    ActorsShared8013231cCoord* coord;

    coord             = (ActorsShared8013231cCoord*)((TmdObject*)task->extra)->coords;
    coord->coord.t[0] = args->pos.vx;
    coord->coord.t[1] = args->pos.vy;
    coord->coord.t[2] = args->pos.vz;
    coord->rot.vx     = args->rot.vx;
    coord->rot.vy     = args->rot.vy;
    coord->rot.vz     = args->rot.vz;
    RotMatrix(&coord->rot, &coord->coord);
    coord->flg = 0;
    return 0;
}

/// Message-0x7D5 display handler: switches on the message's mode word, then
/// copies the model's flags onto the child's model. Mode 0 hides the model
/// and clears flag 0x4; 1 shows it, reallocates its buffers through
/// `Tmd_AllocBuffers` and clears 0x4; 2 hides it, sets 0x4 and starts the
/// work block's countdown at 2, after which the tick frees the buffers; 3
/// shows it and sets 0x4. The handled modes return 0; any other mode changes
/// nothing on this model and returns 1.
s32 func_actor_213100_8014A40C(Task* task, s32 arg1, s32 mode)
{
    TmdObject*       obj;
    TmdObject*       other;
    Actor213100Work* work;
    s32              ret;

    obj   = (TmdObject*)task->extra;
    work  = (Actor213100Work*)task->work;
    other = (TmdObject*)work->field_480->extra;
    ret   = 0;
    switch (mode) {
        case 0:
            obj->flags |= 0x80;
            obj->flags &= ~4;
            break;
        case 1:
            obj->flags &= ~0x80;
            Tmd_AllocBuffers(obj);
            obj->flags &= ~4;
            break;
        case 2:
            obj->flags     |= 0x80;
            work->field_484 = mode;
            obj->flags     |= 4;
            break;
        case 3:
            obj->flags &= ~0x80;
            obj->flags |= 4;
            break;
        default:
            ret = 1;
            break;
    }
    other->flags = obj->flags;
    return ret;
}
