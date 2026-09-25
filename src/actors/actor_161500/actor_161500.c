#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "actors/actor.h"
#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3688.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3E9C.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

extern u8 D_80072729;

extern TaskDesc D_actor_161500_801401B0[];
extern u8       D_actor_161500_801401C8[];
extern u8       D_actor_161500_80140180[];

extern s32 D_actor_161500_80134920[8];
extern s32 D_actor_161500_80135288[8];
extern s32 D_actor_161500_801352A8;
extern s32 D_actor_161500_801354B8;
extern s32 D_actor_161500_80135668;
extern s32 D_actor_161500_801357E8;
extern s32 D_actor_161500_80135968;
extern s32 D_actor_161500_80135AE8;
extern s32 D_actor_161500_80135C68;
extern s32 D_actor_161500_80136E88;
extern s32 D_actor_161500_80137080;
extern s32 D_actor_161500_80137650;
extern s16 D_actor_161500_801376F2;
extern s32 D_actor_161500_801376F8;
extern s32 D_actor_161500_801378D8;
extern s32 D_actor_161500_80137AB8;

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */
#define SCRATCH_SP (*(u32*)0x1F8003FC)

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Work block the actor's spawn routine allocates with `memCalloc(0x4FC, 0)`
/// and hangs off its task's `Task::work` slot. `light` / `color` are the
/// matrix pair the actor's model and its sub-model are lit with, and `anim`,
/// `slots` and `pose` are what `func_800B3F84` fills in.
///
/// The spawn routine optionally spawns a paired enemy, parks that spawn's task
/// in `pairTask` and reparents its own task under it, so the two share one
/// sub-model chain; `enemy` is the enemy its own task belongs to.
///
/// `state` selects what the step body does next: 1 and 2 reseed the animation
/// slots from `animId` (with and without `animArg`) and advance to 3, which
/// ticks them. `appliedAnimId` records the id the slots were last seeded with.
/// `animId` 4 is the walk clip, which `travel` counts down in steps of 30.
/// `yaw` caches the heading the placement and walk-to opcodes last gave the
/// root coordinate. `field_4F0` is the weight, 0 to 0x1000, of the per-frame
/// head turn toward the player, ramped up while `field_4EE` is 1 and down
/// otherwise.
typedef struct Actor161500Work {
    /* 0x000 */ MATRIX     light;
    /* 0x020 */ MATRIX     color;
    /* 0x040 */ GpAnimCtx  anim;
    /* 0x054 */ GpAnimSlot slots[0x14];
    /* 0x374 */ byte       pose[0x140];
    /* 0x4B4 */ s16        state;
    /* 0x4B6 */ s16        appliedAnimId;
    /* 0x4B8 */ s16        animId;
    /* 0x4BA */ s16        field_4BA;
    /* 0x4BC */ byte       pad_4BC[0x2A];
    /* 0x4E6 */ u16        yaw;
    /* 0x4E8 */ byte       pad_4E8[0x2];
    /* 0x4EA */ s16        travel;
    /* 0x4EC */ s16        animArg;
    /* 0x4EE */ s16        field_4EE;
    /* 0x4F0 */ s16        field_4F0;
    /* 0x4F2 */ byte       pad_4F2[0x2];
    /* 0x4F4 */ Task*      pairTask;
    /* 0x4F8 */ GpEnemy*   enemy;
} Actor161500Work;
STATIC_ASSERT_SIZEOF(Actor161500Work, 0x4FC);

/// Payload of the script opcode that writes the work block's `field_4EE`.
typedef struct Actor161500FlagArgs {
    /* 0x0 */ byte pad_0[2];
    /* 0x2 */ u16  value;
} Actor161500FlagArgs;

void func_actor_161500_8013252C(Task* task);
void func_actor_161500_8013273C(GpEnemy* enemy, Task* task);
void func_actor_161500_8013284C(Task* task);
void func_actor_161500_80132874(Task* task);
void func_actor_161500_80132900(Task* task);
void func_actor_161500_8013294C(Task* task);
void func_actor_161500_801329C4(Task* task);

void func_actor_161500_80131E38(void)
{
    if ((GameFlag_GetNibble(0x116) != 1) && (GameFlag_GetNibble(0x116) != 2) && (GameFlag_GetNibble(0x113) == 4)) {
        GameFlag_SetNibble(0x113, 5);
        GameFlag_SetNibble(0x116, 4);
    }

    switch (GameFlag_GetNibble(0x116)) {
        case 0:
            func_800E8614((s32)&D_actor_161500_80135668, 0);
            break;
        case 1:
            func_800E8614((s32)&D_actor_161500_801357E8, 0);
            GameFlag_SetNibble(0x116, 3);
            break;
        case 2:
            func_800E8614((s32)&D_actor_161500_80135968, 0);
            GameFlag_SetNibble(0x116, 3);
            break;
        case 3:
            func_800E8614((s32)&D_actor_161500_80135AE8, 0);
            GameFlag_SetNibble(0x116, 0);
            break;
        case 4:
            func_800E8614((s32)&D_actor_161500_80135C68, 0);
            GameFlag_SetNibble(0x116, 0);
            break;
    }
}

void func_actor_161500_80131F50(s32 arg0)
{
    s8 capFile;

    if (arg0 != 0) {
        Gp_CapFile = 0;
        if (arg0 <= 0) {
            capFile = 1;
            if (gGameSession->at4.loc.place == 1) {
                capFile = 2;
            }
            arg0 = capFile;
        }
        Gp_LoadCapFile(arg0);
        func_800E6D4C(0x340, 0);
        return;
    }
    Gp_ResetCap();
}

void func_actor_161500_80131FBC(void)
{
    s32 temp_s0;
    s32 temp_v0;

    temp_s0 = (gGameSession->at4.loc.place == 1) * 4;
    temp_v0 = GameFlag_GetNibble(0x103);
    func_800E8614(D_actor_161500_80134920[temp_v0 + temp_s0], 0);
    if (temp_v0 < 3) {
        GameFlag_SetNibble(0x103, temp_v0 + 1);
    }
}

void func_actor_161500_80132038(void)
{
    s32 temp_s0;
    s32 temp_v0;

    temp_s0 = (gGameSession->at4.loc.place == 1) * 4;
    temp_v0 = GameFlag_GetNibble(0x104);
    func_800E8614(D_actor_161500_80135288[temp_v0 + temp_s0], 0);
    if (temp_v0 < 3) {
        GameFlag_SetNibble(0x104, temp_v0 + 1);
    }
}

void func_actor_161500_801320B4(void)
{
    GameSession* session;

    session = gGameSession;
    do {
        func_800D4D2C((session->at4.loc.place == 1) ? 0x31 : 0x30);
    } while (0);
}

void func_actor_161500_801320F0(s32 arg0)
{
    Gp_RunCapCmd(arg0, 0);
}

void func_actor_161500_80132110(void)
{
    if (GameFlag_GetNibble(0x105) == 0) {
        func_800E8614((s32)&D_actor_161500_801352A8, 0);
    } else {
        func_800E8614((s32)&D_actor_161500_801354B8, 0);
    }
}

void func_actor_161500_80132150(void)
{
    if (GameFlag_GetNibble(0x112) != 0) {
        func_800D4D2C((GameFlag_GetNibble(0xEA) != 2) ? 0x31 : 0x33);
    } else {
        func_800D4D2C((GameFlag_GetNibble(0xEA) == 2) ? 0x32 : 0x30);
    }
}

void func_actor_161500_801321B4(Task* arg0)
{
    D_80115768 = 1;
    Gp_SetItemSeenBit(0x124, 1);
    GameFlag_SetNibble(0xE4, 2);
    func_800E8614((s32)&D_actor_161500_80137AB8, 0);
    taskKill(arg0);
}

void func_actor_161500_80132210(void)
{
    GsCOORDINATE2* target;
    GsCOORDINATE2* player;

    target = ((TmdObject*)(gameGetPtrSlot(0xA))->extra)->coords;
    player = ((TmdObject*)(gameGetPtrSlot(3))->extra)->coords;
    Gp_UpdateCoord(target);
    Gp_UpdateCoord(player);
    D_actor_161500_801376F2 =
        ratan2(target->coord.t[0] - player->coord.t[0], target->coord.t[2] - player->coord.t[2]) & 0xFFF;
}

void func_actor_161500_80132294(u8 arg0)
{
    D_80115768 = arg0;
}

void func_actor_161500_801322A0(void)
{
    s32 temp_v0;

    if (gameGetPtrSlot(0xA) != NULL) {
        temp_v0 = GameFlag_GetNibble(0xE4);
        if (temp_v0 == 1) {
            if (Gp_GetCurBit2Flag(3) == temp_v0) {
                func_800E8614((s32)&D_actor_161500_801378D8, 0);
            } else {
                func_800E8614((s32)&D_actor_161500_801376F8, 0);
            }
        }
    }
}

void func_actor_161500_8013230C(void)
{
    s32 temp_v0;

    if (gameGetPtrSlot(0xA) != NULL) {
        temp_v0 = GameFlag_GetNibble(0xE4);
        switch (temp_v0) {
            case 0:
                func_800E8634((s32)&D_actor_161500_80137080, 0, (s32)&D_actor_161500_80136E88);
                GameFlag_SetNibble(0xE4, 1);
                break;
            case 1:
                func_800E8614((s32)&D_actor_161500_80137650, 1);
                break;
            case 2:
                break;
        }
    }
}

/// The actor's spawn routine: allocates the work block, destroying the enemy
/// if that fails, and installs the exit callback. With `Task::spawnArg1` set it
/// spawns the paired enemy, reparents its own task under the pair's and starts
/// on clip 2, otherwise on clip 1. It then lights the model, sets up the
/// animation context and the task's message table, and runs the step body
/// once with the plain reseed queued.
void func_actor_161500_80132394(GpEnemy* enemy, Task* task)
{
    VECTOR           vec;
    Actor161500Work* work;
    GsCOORDINATE2*   coord;
    TmdObject*       obj;
    GpEnemy*         spawned;

    coord      = ((TmdObject*)task->extra)->coords;
    obj        = task->extra;
    work       = (Actor161500Work*)memCalloc(0x4FC, false);
    task->work = (TaskIdMap*)work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback   = func_actor_161500_8013284C;
    coord->sub           = &gGfxViewCoord;
    enemy->field_4       = &coord->coord;
    enemy->field_48      = 0;
    enemy->node.targeted = 0;
    enemy->node.flags    = 1;
    obj->otOffset        = 1;
    work->enemy          = enemy;
    if (task->spawnArg1 != 0) {
        spawned = Gp_SpawnEnemyFromTable(D_actor_161500_801401B0, 1, 0, enemy);
        Task_Reparent(task, spawned->task);
        work->pairTask = spawned->task;
        work->animId   = 2;
    } else {
        work->animId = 1;
    }
    work->field_4EE = 0;
    work->field_4F0 = 0;
    obj->lightMtx   = &work->light;
    obj->colorMtx   = &work->color;
    vec.vx          = coord->workm.t[0];
    vec.vy          = coord->workm.t[1] - 0x320;
    vec.vz          = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&work->anim, D_actor_161500_801401C8, obj,
                  &work->slots[0x14], work->slots);
    work->state    = 2;
    task->msgTable = D_actor_161500_80140180;
    func_actor_161500_8013252C(task);
    task->state += 1;
}

/// The actor's step body. States 1 and 2 reseed the animation slots (with and
/// without `animArg`) and advance to 3; state 3 walks the root coordinate 30
/// units per frame while the walk clip has `travel` left, and when it runs out
/// queues a reseed into clip 1 with argument 0xA, then ticks the slots.
void func_actor_161500_8013252C(Task* task)
{
    Actor161500Work* work;
    s16              animId;

    work = (Actor161500Work*)task->work;
    if (work->state == 1) {
        func_actor_161500_801329C4(task);
        work->state = 3;
        return;
    }
    if (work->state == 2) {
        func_actor_161500_8013294C(task);
        work->state = 3;
        return;
    }
    if (work->state == 3) {
        do {
        } while (0);
        animId = work->animId;
        if (animId == 4 && work->travel != 0) {
            actorMoveForward(((TmdObject*)task->extra)->coords, 0x1E);
            work->travel = (u16)work->travel - 1;
            if (work->travel == 0) {
                work->state   = 1;
                work->animArg = 0xA;
                work->animId  = 1;
            }
        }
        func_actor_161500_80132900(task);
        return;
    }
}

/// The actor's task body: dispatches on `Task::state` to the spawn routine
/// (state 0) or the per-frame body (state 1), handing each the task's
/// `GpEnemy` from `Task::spawnArg2`. The handler table is built on the stack.
void func_actor_161500_801326E8(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_161500_80132394,
        func_actor_161500_8013273C,
    };

    fns[task->state](task->spawnArg2, task);
}

/// The actor's draw body: refreshes the model root's coordinate, lights the
/// model at its world translation raised by 800 on y, then runs the step body.
/// `field_4F0` is the head-tracking blend rate handed to `func_800B0928`,
/// ramped toward 0x1000 in 0x200 steps while `field_4EE` is 1 and back down to
/// 0 otherwise, so the actor turns its head to the player and away again
/// smoothly instead of snapping.
void func_actor_161500_8013273C(GpEnemy* enemy, Task* task)
{
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    Actor161500Work* work;
    VECTOR           pos;

    obj   = (TmdObject*)task->extra;
    coord = obj->coords;
    work  = (Actor161500Work*)task->work;
    Gp_UpdateCoord(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1] - 0x320;
    pos.vz = coord->workm.t[2];
    func_800D7A9C(obj, &pos, 0, 3);
    func_actor_161500_8013252C(task);
    if (work->field_4EE == 1) {
        work->field_4F0 += 0x200;
        if (work->field_4F0 > 0x1000) {
            work->field_4F0 = 0x1000;
        }
    } else {
        work->field_4F0 -= 0x200;
        if (work->field_4F0 < 0) {
            work->field_4F0 = 0;
        }
    }
    func_800B0928(task, gameGetPtrSlot(3), 0x200, 0x100, work->field_4F0);
    func_actor_161500_80132874(task);
}

/// The actor's `Task::exitCallback`: hands the task's `GpEnemy`, parked in
/// `Task::spawnArg2`, back to `Gp_DestroyEnemy`.
void func_actor_161500_8013284C(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}

/// Draws the actor's ground shadow quad under the model root, unless the model
/// is hidden (`TmdObject::flags` bit 0x80) or has no buffer yet. The world
/// position is the translation of the root coordinate's `workm`, staged in a
/// scratchpad VECTOR3 rather than on the stack.
void func_actor_161500_80132874(Task* task)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    VECTOR3*       vec;

    obj   = (TmdObject*)task->extra;
    coord = obj->coords;
    if (!(obj->flags & 0x80) && obj->buffer != NULL) {
        vec     = (VECTOR3*)(SCRATCH_SP -= 0x18);
        vec->vx = coord->workm.t[0];
        vec->vy = coord->workm.t[1];
        vec->vz = coord->workm.t[2];
        Gp_DrawEffGroundQuad(vec, 0x200, 0xC0);
        SCRATCH_SP += 0x18;
    }
}

/// Ticks animation slots 1..0x13 of the actor's animation context.
void func_actor_161500_80132900(Task* task)
{
    Actor161500Work* work;
    s32              i;

    work = (Actor161500Work*)task->work;
    i    = 1;
    do {
        Gp_AnimTickIndex(&work->anim, i);
        i++;
    } while (i < 0x14);
}

/// Reseeds animation slots 1..0x13 with `animId`, each at rate 1, and records
/// that id as the one applied.
void func_actor_161500_8013294C(Task* task)
{
    Actor161500Work* work;
    s32              i;

    work = (Actor161500Work*)task->work;
    i    = 1;
    do {
        work->slots[i].rate = 1;
        Gp_AnimResetSlot(&work->anim, i, work->animId);
        i++;
    } while (i < 0x14);
    work->appliedAnimId = work->animId;
}

/// Reseeds animation slots 1..0x13 with `animId`, passing `animArg` through,
/// and records that id as the one applied.
void func_actor_161500_801329C4(Task* task)
{
    Actor161500Work* work;
    s32              i;

    work = (Actor161500Work*)task->work;
    i    = 1;
    do {
        func_800B4114(&work->anim, i, work->animId, 0, work->animArg);
        i++;
    } while (i < 0x14);
    work->appliedAnimId = work->animId;
}

/// Script opcode: starts clip `args->field_4` on this actor, rejecting ids of
/// 0xC and above. With `args->field_8` set it queues the reseed that carries
/// `args->field_C` (state 1), otherwise the plain one (state 2), then runs the
/// step body at once so the new clip is seeded this frame.
///
/// The `SOFT_BARRIER` pins the delay slot of the `beqz`: without it the slot
/// fills from the fall-through arm (`state = 1`) instead of the else arm's
/// `state = 2`.
s32 func_actor_161500_80132A28(Task* task, s32 arg1, GpAnimArg* args)
{
    Actor161500Work* work;

    work = (Actor161500Work*)task->work;
    if (args->field_4 >= 0xC) {
        return -1;
    }

    work->animId = args->field_4;
    if (args->field_8 != 0) {
        SOFT_BARRIER();
        work->state   = 1;
        work->animArg = args->field_C;
    } else {
        work->state = 2;
    }
    work->field_4BA = 0;
    func_actor_161500_8013252C(task);
    return 0;
}

/// Script opcode: hides or shows this actor's model and the model of the pair
/// task its spawn routine parked in `pairTask`. Without `flags` bit 0 both
/// models get `TmdObject::flags` 0x80, which hides them; with it the flags are
/// cleared. Bit 1 additionally ORs in 0x4. With no pair spawned
/// (`Task::spawnArg1` == 0) the actor drives its own model twice.
s32 func_actor_161500_80132A94(Task* task, s32 arg1, s32 flags)
{
    Actor161500Work* work;
    TmdObject*       self;
    TmdObject*       other;

    self = (TmdObject*)task->extra;
    work = (Actor161500Work*)task->work;
    if (task->spawnArg1 != 0) {
        other = (TmdObject*)work->pairTask->extra;
    } else {
        other = self;
    }
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

/// Script opcode "place at": yaws the actor's root coordinate to
/// `placement->rot.vy`, caching that yaw in the work block, then drops the
/// placement translation into the matrix and marks it dirty.
s32 func_actor_161500_80132B10(Task* task, s32 arg1, GpXformArg* placement)
{
    GsCOORDINATE2*   coord;
    Actor161500Work* work;
    u16              yaw;

    coord     = ((TmdObject*)task->extra)->coords;
    work      = (Actor161500Work*)task->work;
    yaw       = placement->rot.vy;
    work->yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    coord->flg        = 0;
    return 0;
}

/// Script opcode: sets the work block's `field_4EE`, which selects whether the
/// per-frame body turns the actor's head toward the player or away, to the
/// payload.
s32 func_actor_161500_80132B88(Task* task, s32 arg1, Actor161500FlagArgs* args)
{
    ((Actor161500Work*)task->work)->field_4EE = args->value;
    return 0;
}

/// Script opcode "walk to": aims the actor's root coordinate at `target` by
/// taking the yaw of the horizontal offset from the coordinate's own
/// translation, caches that yaw in the work block and rebuilds the local
/// matrix from it, then records the distance, in steps of 30, for the walk
/// that follows.
s32 func_actor_161500_80132BA0(Task* task, s32 arg1, GpXformArg* target)
{
    GsCOORDINATE2*   coord;
    Actor161500Work* work;
    s32              dx;
    s32              dz;
    u16              yaw;

    coord     = ((TmdObject*)task->extra)->coords;
    work      = (Actor161500Work*)task->work;
    dx        = target->pos.vx - coord->coord.t[0];
    dz        = target->pos.vz - coord->coord.t[2];
    yaw       = ratan2(dx, dz);
    work->yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    work->travel = SquareRoot0(dx * dx + dz * dz) / 30;
    return 0;
}

/// Per-frame task of the actor's sub-model, with the sub-model's own
/// `TmdObject` in `Task::extra` and the actor as `Task::parent`. The first
/// frame lights the sub-model with the matrix pair at the front of the
/// parent's work block and hangs its coordinate off the parent model's eighth
/// coordinate; every frame marks the coordinate dirty.
void func_actor_161500_80132C6C(Task* task)
{
    Task*          parent = task->parent;
    TmdObject*     obj    = task->extra;
    GsCOORDINATE2* coord  = obj->coords;
    GsCOORDINATE2* sub    = &((TmdObject*)parent->extra)->coords[7];
    MATRIX*        work   = (MATRIX*)parent->work;

    switch (task->state) {
        case 0:
            coord->flg    = 0;
            obj->lightMtx = work;
            obj->colorMtx = work + 1;
            coord->sub    = sub;
            task->state++;
            break;
        case 1:
            coord->flg = 0;
            break;
    }
}
