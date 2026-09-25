#include "common.h"

#include "actors/actor.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

/// Work block of the overlay's actor, allocated zeroed by its state-0 handler
/// and kept both in `D_actor_420700_8013EFE0` and at `Task::work`; the task
/// dispatcher republishes it every tick, and every other function in the
/// overlay reaches it through the global. `light` and `color` are the model's
/// matrices and `rig` and `st` its animation rig and state. The actor's own
/// state fields are a ramp: `st.field_6` is the mode message 0x7DB selects (1
/// and 3 rise, 2 falls, 0 leaves it alone), and `st.field_8` the value the
/// ramp walks by 0x80, clamped to 0..0x1000.
typedef struct Actor420700Work {
    MATRIX          light;
    MATRIX          color;
    ActorAnimRig20  rig;
    ActorEnemyState st;
    byte            pad_4EC[0xB4];
} Actor420700Work;
STATIC_ASSERT_SIZEOF(Actor420700Work, 0x5A0);

/// The work block above, published by the task dispatcher
/// `func_actor_420700_80132340` and by the state-0 handler.
extern Actor420700Work* D_actor_420700_8013EFE0;

/// The actor's own task, the `task` the state-0 handler
/// `func_actor_420700_80131E24` is entered with. Its `Task::extra` holds the
/// `TmdObject` whose trailing coordinate array `func_actor_420700_801323D8`
/// hangs the model task's own root off, at frame 4.
extern Task* D_actor_420700_8013EFE4;

/// The first task the state-0 handler spawns, the frame-4 model task
/// `func_actor_420700_801323D8`; the actor's exit callback kills it.
extern Task* D_actor_420700_8013EFE8;

/// The second task the state-0 handler spawns, the frame-8 model task
/// `func_actor_420700_801327EC`.
extern Task* D_actor_420700_8013EFEC;

void func_actor_420700_8013239C(Task* task);
void func_actor_420700_80132478(Task* task);
void func_actor_420700_801324EC(void);
void func_actor_420700_80132538(void);
void func_actor_420700_801325C8(void);

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

extern u8       D_actor_420700_8013EF48[];
extern TaskDesc D_actor_420700_8013EF68[];
extern u8       D_actor_420700_8013EF8C[];
extern s32      D_actor_420700_8013EFF0;
extern s32      D_actor_420700_8013EFF4;

/// Step 0 of the `func_actor_420700_80132340` dispatcher: allocate and publish the
/// work block, spawn the two model tasks, texture the first from the placement
/// the actor was spawned from, then seed the model's matrices and animation
/// context before running the first step body.
void func_actor_420700_80131E24(GpEnemy* enemy, Task* task)
{
    VECTOR         vec;
    GpAreaKey      key;
    GsCOORDINATE2* coord;
    TmdObject*     obj;
    TmdObject*     model;
    GpAreaPlace*   place;
    s32            idx;
    u32            raw;
    GpAreaKey*     keyPtr;
    u8             view;
    GpAreaKey*     sessionKey;
    void*          work;

    obj                     = task->extra.tmd;
    coord                   = obj->coords;
    work                    = memCalloc(0x5A0, 0);
    D_actor_420700_8013EFE0 = work;
    task->work              = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback           = func_actor_420700_8013239C;
    coord->sub                   = &gGfxViewCoord;
    enemy->field_4               = &coord->coord;
    enemy->field_48              = 0;
    enemy->node.state.b.targeted = 0;
    enemy->node.state.b.flags    = 1;
    obj->otOffset                = 1;
    obj->flags                   = 0;
    D_actor_420700_8013EFE4      = task;
    D_actor_420700_8013EFE8      = Task_SpawnFromTable(D_actor_420700_8013EF68, 1, 0, 0);
    D_actor_420700_8013EFEC      = Task_SpawnFromTable(D_actor_420700_8013EF68, 2, 0, 0);
    sessionKey                   = (GpAreaKey*)&gGameSession->at4.loc;
    raw                          = enemy->placeKey;
    model                        = D_actor_420700_8013EFE8->extra.tmd;
    key.stage                    = sessionKey->stage;
    key.area                     = sessionKey->area;
    key.room                     = sessionKey->room;
    view                         = sessionKey->view;
    idx                          = raw >> 12;
    SOFT_BARRIER();
    keyPtr = &key;
    TOUCH_REG(keyPtr);
    key.view = view;
    Gp_SyncAreaKeyIndex(keyPtr);
    place        = (GpAreaPlace*)((idx << 4) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
    model->tpage = place->tpage;
    model->clut  = place->clut;
    if (model->buffer != NULL) {
        tmdProcessStream(model);
        tmdProcessStream(model);
    }
    obj->lightMtx           = &D_actor_420700_8013EFE0->light;
    obj->colorMtx           = &D_actor_420700_8013EFE0->color;
    D_actor_420700_8013EFF0 = 0;
    vec.vx                  = coord->workm.t[0];
    vec.vy                  = coord->workm.t[1] - 0x320;
    D_actor_420700_8013EFF4 = 0x96;
    vec.vz                  = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&D_actor_420700_8013EFE0->rig.anim, D_actor_420700_8013EF8C, obj,
                  D_actor_420700_8013EFE0->rig.poses, D_actor_420700_8013EFE0->rig.slots);
    D_actor_420700_8013EFE0->st.animId = 5;
    D_actor_420700_8013EFE0->st.state  = 2;
    task->msgTable                     = D_actor_420700_8013EF48;
    func_actor_420700_80132478(task);
    task->state++;
}

/// Step 1 of the `func_actor_420700_80132340` dispatcher: refresh the model's third
/// coordinate and colour the actor from its world translation, run
/// `func_actor_420700_80132478`, then step the `st.field_8` ramp by the mode in
/// `st.field_6` and pass it as the weight of `func_800B0928` aimed at the
/// slot-3 task (modes 0, 1 and 2) or of `func_800B0CF4` aimed at a fixed
/// world point (mode 3).
///
/// Mode 0 chooses its own step each frame: +0x40 while the actor lies behind
/// the slot-3 actor's `field_52` heading, -0x80 otherwise or while an event
/// is running. In that mode the animation slots after the first are held
/// (rate 0) once the ramp is off zero; otherwise they run at one frame per
/// tick.
void func_actor_420700_80132064(GpEnemy* enemy, Task* task)
{
    VECTOR         pos;
    GsCOORDINATE2  target[2];
    GsCOORDINATE2* coords;
    GsCOORDINATE2* player;
    GsCOORDINATE2* part;
    GameActor*     actor;
    s32            dx;
    s32            dz;
    s32            c;
    s32            i;
    u8             rate;

    coords = task->extra.tmd->coords;
    part   = &coords[2];
    player = gameGetPtrSlot(3)->extra.tmd->coords;
    Gp_UpdateCoord(part);
    pos.vx = part->workm.t[0];
    pos.vy = part->workm.t[1];
    pos.vz = part->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);
    func_actor_420700_80132478(task);
    rate = 0x10;
    if (D_actor_420700_8013EFE0->st.field_6 != 0) {
        if (D_actor_420700_8013EFE0->st.field_6 == 1 || D_actor_420700_8013EFE0->st.field_6 == 3) {
            D_actor_420700_8013EFE0->st.field_8 += 0x80;
            if (D_actor_420700_8013EFE0->st.field_8 > 0x1000) {
                D_actor_420700_8013EFE0->st.field_8 = 0x1000;
            }
        } else {
            D_actor_420700_8013EFE0->st.field_8 -= 0x80;
            if (D_actor_420700_8013EFE0->st.field_8 < 0) {
                D_actor_420700_8013EFE0->st.field_8 = 0;
            }
        }
        if (D_actor_420700_8013EFE0->st.field_6 == 3) {
            target[0].coord.t[0] = 0x1173;
            target[0].coord.t[1] = 0;
            target[0].coord.t[2] = -0x733;
            func_800B0CF4(task, target, 0x200, 0x100, D_actor_420700_8013EFE0->st.field_8);
        } else {
            func_800B0928(task, gameGetPtrSlot(3), 0x200, 0x100, D_actor_420700_8013EFE0->st.field_8);
        }
    } else {
        if (gGameSession->eventState == 0) {
            dx    = coords->coord.t[0] - player->coord.t[0];
            dz    = coords->coord.t[2] - player->coord.t[2];
            actor = (GameActor*)gameGetPtrSlot(3)->work;
            c     = rcos(actor->field_52);
            if (dx * rsin(actor->field_52) + dz * c < 0) {
                D_actor_420700_8013EFF0 = 0x40;
            } else {
                D_actor_420700_8013EFF0 = -0x80;
            }
            if (D_actor_420700_8013EFE0->st.field_8 != 0) {
                rate = 0;
            }
        } else {
            D_actor_420700_8013EFF0 = -0x80;
        }
        D_actor_420700_8013EFE0->st.field_8 += D_actor_420700_8013EFF0;
        if (D_actor_420700_8013EFE0->st.field_8 > 0x1000) {
            D_actor_420700_8013EFE0->st.field_8 = 0x1000;
        }
        if (D_actor_420700_8013EFE0->st.field_8 < 0) {
            D_actor_420700_8013EFE0->st.field_8 = 0;
        }
        func_800B0928(task, gameGetPtrSlot(3), 0x200, 0x100, D_actor_420700_8013EFE0->st.field_8);
    }
    for (i = 1; i < 0x14; i++) {
        D_actor_420700_8013EFE0->rig.slots[i].rate = rate;
    }
}

/// Task handler of the actor: republishes the task's work block in
/// `D_actor_420700_8013EFE0`, so the rest of the overlay can reach it without
/// the task, then runs the handler for the task's state from a two-entry table
/// built on the stack -- the spawn step `func_actor_420700_80131E24` or the
/// per-frame step `func_actor_420700_80132064`.
void func_actor_420700_80132340(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_420700_80131E24,
        func_actor_420700_80132064,
    };

    D_actor_420700_8013EFE0 = task->work;
    fns[task->state](task->spawnArg2, task);
}

/// Exit callback of the actor's task: kills the frame-4 model task and
/// destroys the enemy.
void func_actor_420700_8013239C(Task* arg0)
{
    taskKill(D_actor_420700_8013EFE8);
    Gp_DestroyEnemy(arg0->spawnArg2, arg0);
}

/// State handler of the frame-4 model task: the spawn tick clears its root
/// coordinate's `flg` and the model's flags, which leaves it visible, and hangs
/// the root off frame 4 of the actor's own model, stepping to state 1; every
/// later tick hands the actor model's root translation, dropped by 0x320 in y,
/// to `func_800D7A9C` for the model's colour matrix.
void func_actor_420700_801323D8(Task* task)
{
    TmdObject*     extra = task->extra.tmd;
    GsCOORDINATE2* coord = extra->coords;
    GsCOORDINATE2* parts = D_actor_420700_8013EFE4->extra.tmd->coords;
    GsCOORDINATE2* part  = parts + 4;
    VECTOR         vec;

    switch (task->state) {
        case 0:
            coord->flg   = 0;
            extra->flags = 0;
            coord->sub   = part;
            task->state++;
            break;
        case 1:
            vec.vx = parts->workm.t[0];
            vec.vy = parts->workm.t[1] - 0x320;
            vec.vz = parts->workm.t[2];
            func_800D7A9C(extra, &vec, 0, 3);
            break;
    }
}

/// Runs the body the actor's step selects and then leaves it in step 3, the
/// running state. Steps 1 and 2 each return through their own copy of the
/// advance; the two are identical, so jump.c cross-jumps them and only the
/// second survives.
void func_actor_420700_80132478(Task* task)
{
    if (D_actor_420700_8013EFE0->st.state == 1) {
        func_actor_420700_801325C8();
        D_actor_420700_8013EFE0->st.state = 3;
        return;
    }
    if (D_actor_420700_8013EFE0->st.state == 2) {
        func_actor_420700_80132538();
        D_actor_420700_8013EFE0->st.state = 3;
        return;
    }
    if (D_actor_420700_8013EFE0->st.state == 3) {
        func_actor_420700_801324EC();
    }
}

/// Advances animation slots 1..0x13 of the work block by one tick.
void func_actor_420700_801324EC(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickIndex(&D_actor_420700_8013EFE0->rig.anim, i);
        i++;
    } while (i < 0x14);
}

/// Sets the rate of animation slots 1..0x13 to 1 and resets each of them to
/// the current animation id, then records that id as the one now playing.
void func_actor_420700_80132538(void)
{
    s32 i;

    i = 1;
    do {
        D_actor_420700_8013EFE0->rig.slots[i].rate = 1;
        Gp_AnimResetSlot(&D_actor_420700_8013EFE0->rig.anim, i, D_actor_420700_8013EFE0->st.animId);
        i++;
    } while (i < 0x14);
    D_actor_420700_8013EFE0->st.appliedAnimId = D_actor_420700_8013EFE0->st.animId;
}

/// Reseeds animation slots 1..0x13 from the current animation id and records
/// that id as the one now playing.
void func_actor_420700_801325C8(void)
{
    s32 i;

    i = 1;
    do {
        func_800B4114(&D_actor_420700_8013EFE0->rig.anim, i, D_actor_420700_8013EFE0->st.animId, 0, 8);
        i++;
    } while (i < 0x14);
    D_actor_420700_8013EFE0->st.appliedAnimId = D_actor_420700_8013EFE0->st.animId;
}

/// Message 0x7D3 handler: selects animation `field_4` (below 0x15) of the bank
/// `field_0` picks -- ids from 0 for bank 0, 0xA for bank 1, 0x11 for bank 2 --
/// and sets the actor step to 1 (reseed through `func_800B4114`) when `field_8`
/// is non-zero or 2 (plain slot reset) otherwise, then runs the step at once on
/// the actor's own task. Returns 0, or -1 for an index out of range.
s32 func_actor_420700_80132644(Task* task, s32 arg1, GpAnimArg* args)
{
    s32              offset;
    Actor420700Work* work;

    if (args->field_4 < 0x15) {
        switch (args->animBlock.index) {
            case 1:
                offset = 0xA;
                break;
            case 2:
                offset = 0x11;
                break;
            default:
                offset = 0;
                break;
        }
        work            = D_actor_420700_8013EFE0;
        work->st.animId = (u16)args->field_4 + offset;
        if (args->field_8 != 0) {
            work->st.state = 1;
        } else {
            work->st.state = 2;
        }
        D_actor_420700_8013EFE0->st.field_A = 0;
        func_actor_420700_80132478(D_actor_420700_8013EFE4);
        return 0;
    }
    return -1;
}

/// Message 0x7D5 handler: rewrites the flags of the actor's three models -- its
/// own and those of the frame-4 and frame-8 model tasks. Bit 0 of the argument
/// shows all three (flags 0) when set and hides them (0x80) when clear; bit 1
/// then ORs 0x4 into all three. Always returns 0.
///
/// The argument is the handler table's third slot, not the second, so the three
/// objects it loads land in `$a3` / `$a0` / `$v1` rather than shifted one down.
s32 func_actor_420700_801326F4(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* actor = D_actor_420700_8013EFE4->extra.tmd;
    TmdObject* model = D_actor_420700_8013EFE8->extra.tmd;
    TmdObject* twin  = D_actor_420700_8013EFEC->extra.tmd;

    if (arg2 & 1) {
        actor->flags = 0;
        model->flags = 0;
        twin->flags  = 0;
    } else {
        actor->flags = 0x80;
        model->flags = 0x80;
        twin->flags  = 0x80;
    }
    if (arg2 & 2) {
        actor->flags |= 4;
        model->flags |= 4;
        twin->flags  |= 4;
    }
    return 0;
}

/// Message 0x7DB handler: records the `st.field_6` mode the ramp
/// `func_actor_420700_80132064` runs and seeds `st.field_8` at the end that mode
/// walks away from -- 0 for the rising modes 1 and 3, 0x1000 for the falling
/// mode 2. Mode 0 is accepted as a no-op, and a block whose leading id is not
/// 0x1B02 is rejected with -1 without touching the work block.
///
/// The empty `case 0` is what the decision tree is built from: with the three
/// live cases alone GCC balances the list at the middle node and comes out one
/// test short, and adding the fourth node is what makes it split at the first
/// case instead. See DECOMPILATION_LEARNINGS.md, "An empty case node changes
/// the switch decision tree".
s32 func_actor_420700_80132784(Task* task, s32 arg1, GpCmdArg* args)
{
    if (args->from.key != 0x1B02) {
        return -1;
    }
    D_actor_420700_8013EFE0->st.field_6 = args->command;
    switch (args->command) {
        case 0:
            break;
        case 1:
        case 3:
            D_actor_420700_8013EFE0->st.field_8 = 0;
            break;
        case 2:
            D_actor_420700_8013EFE0->st.field_8 = 0x1000;
            break;
    }
    return 0;
}

/// State handler of the frame-8 model task: the same as the frame-4 one,
/// `func_actor_420700_801323D8`, except that it hangs its root off frame 8 of
/// the actor's own model and its spawn tick also sets the model's `otOffset` to
/// -2.
void func_actor_420700_801327EC(Task* task)
{
    TmdObject*     extra = task->extra.tmd;
    GsCOORDINATE2* coord = extra->coords;
    GsCOORDINATE2* parts = D_actor_420700_8013EFE4->extra.tmd->coords;
    GsCOORDINATE2* part  = parts + 8;
    VECTOR         vec;

    switch (task->state) {
        case 0:
            coord->flg      = 0;
            extra->flags    = 0;
            extra->otOffset = -2;
            coord->sub      = part;
            task->state++;
            break;
        case 1:
            vec.vx = parts->workm.t[0];
            vec.vy = parts->workm.t[1] - 0x320;
            vec.vz = parts->workm.t[2];
            func_800D7A9C(extra, &vec, 0, 3);
            break;
    }
}
