#include "common.h"

#include "actors/actor.h"
#include "actors/actors_shared_8013411c.h"
#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

/// Work block of the overlay's actor, allocated zeroed by its spawn routine
/// and kept both in `D_actor_146300_80142828` and at `Task::work`; every other
/// function in the overlay reaches it through the global. `light` and `color`
/// are the matrices the spawn routine hands the model, and `rig` and `st` its
/// animation rig and state.
typedef struct Actor146300Work {
    MATRIX          light;
    MATRIX          color;
    ActorAnimRig20  rig;
    ActorEnemyState st;
} Actor146300Work;
STATIC_ASSERT_SIZEOF(Actor146300Work, 0x4EC);

/// The work block above, published by the task handler
/// `func_actor_146300_801326CC` and by the spawn routine.
extern Actor146300Work* D_actor_146300_80142828;

/// The actor's own task, published by the spawn routine: the 0x7D3 handler
/// runs the per-frame update on it, and the 0x7D5 handler and the companion's
/// handler `func_actor_146300_80132B1C` reach the actor's model through its
/// `extra`.
extern Task* D_actor_146300_8014282C;

/// Reset argument `func_actor_146300_8013291C` forwards: the 0x7D3 handler
/// latches the preset's `field_C` here.
extern s16 D_actor_146300_8014279C;

/// The companion task the spawn routine starts from
/// `D_actor_146300_801427C8`; its `extra` is the model whose texture page and
/// CLUT row come out of the area record, and the actor's own task is reparented
/// under it.
extern Task* D_actor_146300_80142830;

/// Spawn table of the actor's two tasks: index 0 runs
/// `func_actor_146300_801326CC`, index 1 the companion's
/// `func_actor_146300_80132B1C`, which the spawn routine starts.
extern TaskDesc D_actor_146300_801427C8[];

/// Animation stream the spawn routine binds into the work block's animation
/// context with `func_800B3F84`.
extern u8 D_actor_146300_801427E0[];

/// Message handler table the spawn routine publishes as `Task::msgTable`.
extern GpMsgEntry D_actor_146300_801427A0[];

extern s32 D_actor_146300_80137AAC;
extern s32 D_actor_146300_80137B10;
extern s32 D_actor_146300_80137B38;
extern s32 D_actor_146300_80137B60;
extern s32 D_actor_146300_80137C10;
extern s32 D_actor_146300_801386C0;
extern s32 D_actor_146300_80138810;
extern s32 D_actor_146300_801388D0;
extern s32 D_actor_146300_80138A38;
extern s32 D_actor_146300_80138AC8;
extern s32 D_actor_146300_80142824;

/// `func_800B4114` is declared locally with a signed `arg2`; see the note in
/// `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

void func_actor_146300_80132728(GpEnemy* enemy, Task* task);
void func_actor_146300_801327A4(Task* task);
void func_actor_146300_801327CC(Task* task);
void func_actor_146300_80132840(void);
void func_actor_146300_8013288C(void);
void func_actor_146300_8013291C(void);

void func_actor_146300_80131ECC(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_AgeFlag119();
            switch (GameFlag_GetNibble(0x7B)) {
                case 2:
                    if (Gp_HasCollectedBit(0x119) == 0) {
                        Gp_RunCapCmd1(0x12);
                        task->state++;
                    } else {
                        Gp_ClearCollectedBit(0x119);
                        D_actor_146300_80142824 = 0x13;
                        GameFlag_SetNibble(0x7B, 3);
                        task->state = 0xA;
                    }
                    break;
                case 3:
                    if (Gp_HasCollectedBit(0x119) == 0) {
                        if (Gp_GetCurBit2Flag(0x1F) == 1) {
                            Gp_DispatchMsg(gameGetPtrSlot(6), 0xFA4, 0, 0);
                            D_actor_146300_80142824 = 0x13;
                            task->state             = 0x14;
                        } else {
                            Gp_RunCapCmd1(0x12);
                            task->state++;
                        }
                    } else {
                        Gp_ClearCollectedBit(0x119);
                        D_actor_146300_80142824 = 0x14;
                        GameFlag_SetNibble(0x7B, 4);
                        task->state = 0xA;
                    }
                    break;
                case 4:
                    if (Gp_HasCollectedBit(0x119) == 0) {
                        if (Gp_GetCurBit2Flag(0x20) == 1) {
                            Gp_DispatchMsg(gameGetPtrSlot(6), 0xFA4, 0, 0);
                            D_actor_146300_80142824 = 0x14;
                            task->state             = 0x14;
                        } else {
                            Gp_RunCapCmd1(0x12);
                            task->state++;
                        }
                    } else {
                        Gp_ClearCollectedBit(0x119);
                        GameFlag_SetNibble(0x7B, 5);
                        task->state = 0x1E;
                    }
                    break;
                case 5:
                    if (Gp_GetCurBit2Flag(0x21) == 1) {
                        Gp_DispatchMsg(gameGetPtrSlot(6), 0xFA4, 0, 0);
                        task->state = 0x28;
                    } else {
                        func_800E8614((s32)&D_actor_146300_80138AC8, 0);
                        taskKill(task);
                    }
                    break;
                default:
                    task->state++;
                    break;
            }
            break;
        case 1:
            Gp_MsgPlayerWeapon(1);
            taskKill(task);
            break;
        case 10:
            Gp_StartCapSlot((s16)D_actor_146300_80142824, 0, 0);
            func_800E8614((s32)&D_actor_146300_801386C0, 1);
            task->state++;
            break;
        case 11:
            if (gGameSession->eventState == 0) {
                task->state = 0x14;
            }
            break;
        case 20:
            Gp_StartCapSlot((s16)D_actor_146300_80142824, 0, 1);
            func_800E8614((s32)&D_actor_146300_80138810, 1);
            task->state++;
            break;
        case 30:
            func_800E8614((s32)&D_actor_146300_801388D0, 1);
            task->state++;
            break;
        case 31:
            if (gGameSession->eventState == 0) {
                task->state = 0x28;
            }
            break;
        case 40:
            Gp_StartCapSlot(0x15, 0, 1);
            func_800E8614((s32)&D_actor_146300_80138A38, 1);
            task->state++;
            break;
        case 21:
        case 41:
            if (gGameSession->eventState == 0) {
                task->state = 1;
            }
            break;
    }
}

void func_actor_146300_8013224C(void)
{
    switch (GameFlag_GetNibble(0x7B)) {
        case 2:
            Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7D3, (s32)&D_actor_146300_80137B38, 0);
            break;
        case 3:
            if (Gp_HasCollectedBit(0x119) == 0) {
                if (Gp_GetCurBit2Flag(0x1F) == 1) {
                    Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7D3, (s32)&D_actor_146300_80137AAC, 0);
                } else {
                    Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7D3, (s32)&D_actor_146300_80137B38, 0);
                }
            } else {
                Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7D3, (s32)&D_actor_146300_80137B38, 0);
            }
            break;
        case 4:
            if (Gp_HasCollectedBit(0x119) == 0) {
                if (Gp_GetCurBit2Flag(0x20) == 1) {
                    Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7D3, (s32)&D_actor_146300_80137AAC, 0);
                } else {
                    Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7D3, (s32)&D_actor_146300_80137B38, 0);
                }
                break;
            }
            /* fallthrough */
        case 5:
            Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7D4, (s32)&D_actor_146300_80137C10, 0);
            Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7D3, (s32)&D_actor_146300_80137B60, 0);
            break;
    }
}

void func_actor_146300_801323E0(void)
{
    Gp_PulseState1C();
    Gp_StateC08.field_6 |= 1;
}

void func_actor_146300_80132418(s32 arg0)
{
    switch (arg0) {
        case 0:
            if (Gp_GetCapEventKey() == 1) {
                Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7D3, (s32)&D_actor_146300_80137B10, 0);
            }
            break;
        case 1:
            if (Gp_GetCapEventKey() == 2) {
                Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7D3, (s32)&D_actor_146300_80137AAC, 0);
            }
            break;
    }
}

/// Spawn routine, state 0 of the task handler `func_actor_146300_801326CC`:
/// allocates the 0x4EC work block and publishes it in `D_actor_146300_80142828`
/// and the task's `work` slot (destroying the enemy if the allocation fails),
/// installs the exit callback, binds the model's coordinate frame to the view
/// and publishes the task in `D_actor_146300_8014282C`.
///
/// The companion task from `D_actor_146300_801427C8` carries the model whose
/// texture page and CLUT row come out of the current area record - the session
/// location key is copied onto the stack, `Gp_SyncAreaKeyIndex` fills in its
/// nested index and the enemy's `placeKey >> 12` selects the 0x10-byte record.
/// The actor's task is then reparented under that companion, the model gets the
/// block's light and colour matrices and is relit from a point 0x320 above its
/// root translation, the animation stream is bound, the animation state is
/// seeded with mode 2 / id 0xB, the message table is published and the
/// per-frame update runs once before the state advances.
void func_actor_146300_801324AC(GpEnemy* enemy, Task* task)
{
    VECTOR           vec;
    GpAreaKey        key;
    Actor146300Work* work;
    TmdObject*       obj;
    TmdObject*       model;
    GsCOORDINATE2*   coord;
    GpAreaKey*       sessionKey;
    GpAreaPlace*     entry;
    Task*            helper;
    GpAreaKey*       keyPtr;
    u8               areaByte0;
    u32              idx;

    obj                     = task->extra.tmd;
    coord                   = obj->coords;
    work                    = memCalloc(0x4EC, 0);
    D_actor_146300_80142828 = work;
    task->work              = (TaskIdMap*)work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback           = func_actor_146300_801327A4;
    coord->sub                   = &gGfxViewCoord;
    enemy->field_4               = &coord->coord;
    enemy->field_48              = 0;
    enemy->node.state.b.targeted = 0;
    enemy->node.state.b.flags    = 1;
    obj->otOffset                = 1;
    obj->flags                   = 0;
    D_actor_146300_8014282C      = task;
    helper                       = Task_SpawnFromTable(D_actor_146300_801427C8, 1, 0, 0);
    sessionKey                   = (GpAreaKey*)&gGameSession->at4.loc;
    idx                          = enemy->placeKey >> 0xC;
    model                        = helper->extra.tmd;
    D_actor_146300_80142830      = helper;
    key.stage                    = sessionKey->stage;
    key.area                     = sessionKey->area;
    key.room                     = sessionKey->room;
    areaByte0                    = sessionKey->view;
    /* Both calls take `&key`. Left alone, GCC 2.8.1 CSEs that address into one
       pseudo that is live across the first call, costing a callee-saved
       register; the ROM rematerializes `addiu a0, sp, key` for each call. The
       barrier keeps the address materialization next to the call and the
       `+r` touch makes the second one a fresh computation. */
    SOFT_BARRIER();
    keyPtr = &key;
    TOUCH_REG(keyPtr);
    key.view = areaByte0;
    Gp_SyncAreaKeyIndex(keyPtr);
    /* offset + base, not `&rec->field_0[idx]`: the ROM adds the scaled index
       onto the table (`addu s0, s0, v0`). */
    entry        = (GpAreaPlace*)((idx << 4) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
    model->tpage = entry->tpage;
    model->clut  = entry->clut;
    if (model->buffer != NULL) {
        tmdProcessStream(model);
        tmdProcessStream(model);
    }
    Task_Reparent(task, D_actor_146300_80142830);
    obj->lightMtx = &D_actor_146300_80142828->light;
    obj->colorMtx = &D_actor_146300_80142828->color;
    vec.vx        = coord->workm.t[0];
    vec.vy        = coord->workm.t[1] - 0x320;
    vec.vz        = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&D_actor_146300_80142828->rig.anim, D_actor_146300_801427E0, obj,
                  &D_actor_146300_80142828->rig.poses[0], D_actor_146300_80142828->rig.slots);
    D_actor_146300_80142828->st.animId = 0xB;
    D_actor_146300_80142828->st.state  = 2;
    task->msgTable                     = D_actor_146300_801427A0;
    func_actor_146300_801327CC(task);
    SOFT_BARRIER();
    task->state++;
}

/// The actor's task handler: publishes the task's work block in
/// `D_actor_146300_80142828` on the way through, then runs the handler its
/// state selects from a table built on the stack - the spawn routine for state
/// 0, the per-frame update after it.
void func_actor_146300_801326CC(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_146300_801324AC,
        func_actor_146300_80132728,
    };

    D_actor_146300_80142828 = task->work;
    fns[task->state](task->spawnArg2, task);
}

/// State 1 of the task handler `func_actor_146300_801326CC`: refreshes the model
/// root's world matrix, relights the model from a point 0x320 above its
/// translation, then runs the per-frame update.
void func_actor_146300_80132728(GpEnemy* enemy, Task* task)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    VECTOR         vec;

    obj   = task->extra.tmd;
    coord = obj->coords;
    Gp_UpdateCoord(coord);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1] - 0x320;
    vec.vz = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_actor_146300_801327CC(task);
}

/// `Task::exitCallback` the spawn routine installs: hands the task's `GpEnemy`
/// (parked in `Task::spawnArg2`) back to `Gp_DestroyEnemy`.
void func_actor_146300_801327A4(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}

/// Per-frame update: reset mode 1 runs the reseed with the latched reset
/// argument and mode 2 the plain reseed, each then switching to mode 3; mode 3
/// ticks the animation. Steps 1 and 2 each return through their own copy of the
/// switch to mode 3; the two are identical, so jump.c cross-jumps them and only
/// the second survives.
void func_actor_146300_801327CC(Task* task)
{
    if (D_actor_146300_80142828->st.state == 1) {
        func_actor_146300_8013291C();
        D_actor_146300_80142828->st.state = 3;
        return;
    }
    if (D_actor_146300_80142828->st.state == 2) {
        func_actor_146300_8013288C();
        D_actor_146300_80142828->st.state = 3;
        return;
    }
    if (D_actor_146300_80142828->st.state == 3) {
        func_actor_146300_80132840();
    }
}

/// Ticks animation slots 1..0x13 of the work block's animation context.
void func_actor_146300_80132840(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickIndex(&D_actor_146300_80142828->rig.anim, i);
        i++;
    } while (i < 0x14);
}

/// Marks animation slots 1..0x13 of the work block reset-pending and reseeds
/// each of them from the current animation id, then records that id as the one
/// now playing.
void func_actor_146300_8013288C(void)
{
    s32 i;

    i = 1;
    do {
        D_actor_146300_80142828->rig.slots[i].rate = 1;
        Gp_AnimResetSlot(&D_actor_146300_80142828->rig.anim, i, D_actor_146300_80142828->st.animId);
        i++;
    } while (i < 0x14);
    D_actor_146300_80142828->st.appliedAnimId = D_actor_146300_80142828->st.animId;
}

/// Reseeds animation slots 1..0x13 of the work block from the current animation
/// id with the latched reset argument `D_actor_146300_8014279C`, and records
/// that id as the one now playing.
void func_actor_146300_8013291C(void)
{
    s32 i;

    i = 1;
    do {
        func_800B4114(&D_actor_146300_80142828->rig.anim, i, D_actor_146300_80142828->st.animId, 0,
                      D_actor_146300_8014279C);
        i++;
    } while (i < 0x14);
    D_actor_146300_80142828->st.appliedAnimId = D_actor_146300_80142828->st.animId;
}

/// Message 0x7D3 handler: adopts `preset`'s animation id when it is
/// one of the first 0x11, latching the reset mode and the reset argument the
/// reseed forwards, then hands the published task to the per-frame update. Ids
/// past the range are rejected with -1 and leave the work block untouched.
s32 func_actor_146300_8013299C(Task* task, s32 arg1, GpAnimArg* preset)
{
    if (preset->field_4 < 0x11) {
        D_actor_146300_80142828->st.animId = preset->field_4;
        if (preset->field_8 != 0) {
            D_actor_146300_80142828->st.state = 1;
            D_actor_146300_8014279C           = preset->field_C;
        } else {
            D_actor_146300_80142828->st.state = 2;
        }
        D_actor_146300_80142828->st.field_6 = 0;
        func_actor_146300_801327CC(D_actor_146300_8014282C);
        return 0;
    }
    return -1;
}

/// Message 0x7D5 handler: shows or hides the actor's model and its companion's
/// together. Bit 0 of `flags` clears both models' `TmdObject::flags` (shown);
/// without it both get 0x80 (hidden). Bit 1 additionally ORs in 0x4 on both.
s32 func_actor_146300_80132A2C(Task* task, s32 arg1, s32 flags)
{
    TmdObject* self;
    TmdObject* other;

    self  = D_actor_146300_8014282C->extra.tmd;
    other = D_actor_146300_80142830->extra.tmd;

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

/// Message 0x7D4 handler: turns the model root to `placement`'s yaw
/// (recorded in the work block's `yaw`), moves it to `placement`'s position and
/// marks the coordinate for recomputation. Only the Y rotation is applied.
s32 func_actor_146300_80132A98(Task* task, s32 arg1, GpXformArg* placement)
{
    GsCOORDINATE2* coord;
    u16            yaw;

    coord                           = task->extra.tmd->coords;
    D_actor_146300_80142828->st.yaw = yaw = placement->rot.vy;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    coord->flg        = 0;
    return 0;
}

/// Message 0x7DB handler: accepts the message and does nothing.
s32 func_actor_146300_80132B14(void)
{
    return 0;
}

/// Task handler of the companion task: the first tick hangs the companion
/// model's coordinate frame under part 4 of the actor's model, shows the model
/// and steps to state 1; every later tick relights the companion model from a
/// point 0x320 above the actor model's root translation.
void func_actor_146300_80132B1C(Task* task)
{
    TmdObject*     extra = task->extra.tmd;
    GsCOORDINATE2* coord = extra->coords;
    GsCOORDINATE2* parts = D_actor_146300_8014282C->extra.tmd->coords;
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
