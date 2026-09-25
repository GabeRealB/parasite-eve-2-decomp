#include "common.h"

#include "actors/actor_460200.h"
#include "actors/actors_shared_80132514.h"
#include "actors/actors_shared_8014c874.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

/// Script opcode "walk to": aims the actor's root coordinate at `target` by
/// taking the yaw of the horizontal offset from the coordinate's own
/// translation, caches that yaw in the work block and rebuilds the local
/// matrix from it, then records the distance, in steps of 12, for the step
/// body to walk off.
s32 func_actor_460200_80132CAC(Task* task, s32 arg1, Actor460200WalkTarget* target)
{
    GsCOORDINATE2*   coord;
    Actor460200Work* work;
    s32              dx;
    s32              dz;
    u16              yaw;

    coord     = ((TmdObject*)task->extra)->coords;
    work      = (Actor460200Work*)task->work;
    dx        = target->pos.vx - coord->coord.t[0];
    dz        = target->pos.vz - coord->coord.t[2];
    yaw       = ratan2(dx, dz);
    work->yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    work->travel = SquareRoot0(dx * dx + dz * dz) / 12;
    return 0;
}

extern TaskDesc D_actor_460200_80148118[];
extern u8       D_actor_460200_80148130[];
extern u8       D_actor_460200_801480E8[];

void func_actor_460200_8013322C(Task* task);
void func_actor_460200_80132F0C(Task* task);

void func_actor_460200_80132D74(GpEnemy* enemy, Task* task)
{
    VECTOR               vec;
    Actor460200PairWork* work;
    GsCOORDINATE2*       coord;
    TmdObject*           obj;
    GpEnemy*             spawned;

    coord      = ((TmdObject*)task->extra)->coords;
    obj        = task->extra;
    work       = (Actor460200PairWork*)memCalloc(0x4FC, false);
    task->work = (TaskIdMap*)work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback   = func_actor_460200_8013322C;
    coord->sub           = &gGfxViewCoord;
    enemy->field_4       = &coord->coord;
    enemy->field_48      = 0;
    enemy->node.targeted = 0;
    enemy->node.flags    = 1;
    obj->otOffset        = 1;
    work->enemy          = enemy;
    if (task->spawnArg1 != 0) {
        spawned = Gp_SpawnEnemyFromTable(D_actor_460200_80148118, 1, 0, enemy);
        Task_Reparent(task, spawned->task);
        work->field_4F4 = spawned->task;
        work->animId    = 2;
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
    func_800B3F84(&work->anim, D_actor_460200_80148130, obj,
                  work->pose, work->slots);
    work->state    = 2;
    task->msgTable = D_actor_460200_801480E8;
    func_actor_460200_80132F0C(task);
    task->state += 1;
}

void func_actor_460200_801333A4(Task* task);
void func_actor_460200_8013332C(Task* task);
void func_actor_460200_801332E0(Task* task);

/// The same step body as `ActorsShared8014c874`, moving 0x1E per frame and
/// also returning to state 1 when the travel count runs out.
void func_actor_460200_80132F0C(Task* task)
{
    ActorsShared8014c874Work* work;
    s16                       animId;

    work = (ActorsShared8014c874Work*)task->work;
    if (work->state == 1) {
        func_actor_460200_801333A4(task);
        work->state = 3;
        return;
    }
    if (work->state == 2) {
        func_actor_460200_8013332C(task);
        work->state = 3;
        return;
    }
    if (work->state == 3) {
        do {
        } while (0);
        animId = work->animId;
        if (animId == 4 && work->travel != 0) {
            Actor460200_MoveForward(((TmdObject*)task->extra)->coords, 0x1E);
            work->travel = (u16)work->travel - 1;
            if (work->travel == 0) {
                work->state   = 1;
                work->animArg = 0xA;
                work->animId  = 1;
            }
        }
        func_actor_460200_801332E0(task);
        return;
    }
}

void func_actor_460200_80133254(Task* task);
void func_actor_460200_8013311C(void* enemy, Task* task);

void func_actor_460200_801330C8(Task* task)
{
    void (*fns[2])(void*, Task*) = { func_actor_460200_80132D74, func_actor_460200_8013311C };

    fns[task->state](task->spawnArg2, task);
}

/// Per-tick state 1 of this actor: faces the model toward the `gameGetPtrSlot(3)`
/// task. The root coordinate of the model is updated, a copy of its translation
/// lifted by 0x320 is used as the look-at point, and the work block's
/// `field_4F0` rate is stepped +0x200 or -0x200 per tick depending on
/// `field_4EE`, clamped to 0x1000 and 0 respectively.
void func_actor_460200_8013311C(void* enemy, Task* task)
{
    Actor460200Work* work;
    GsCOORDINATE2*   coord;
    TmdObject*       obj;
    VECTOR           vec;
    u16              angle;

    obj   = task->extra;
    coord = obj->coords;
    work  = (Actor460200Work*)task->work;
    Gp_UpdateCoord(coord);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1] - 0x320;
    vec.vz = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_actor_460200_80132F0C(task);
    if (work->field_4EE == 1) {
        angle           = work->field_4F0 + 0x200;
        work->field_4F0 = angle;
        if ((s16)angle >= 0x1001) {
            work->field_4F0 = 0x1000;
        }
    } else {
        angle           = work->field_4F0 - 0x200;
        work->field_4F0 = angle;
        if ((s16)angle < 0) {
            work->field_4F0 = 0;
        }
    }
    func_800B0928(task, gameGetPtrSlot(3), 0x200, 0x100, (s16)work->field_4F0);
    func_actor_460200_80133254(task);
}

void func_actor_460200_8013322C(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}

/// Draws the actor's ground shadow under its root part, unless the model is
/// hidden (`flags` bit 0x80) or has no buffer. The position is the root part's
/// world translation, staged on the scratchpad stack.
void func_actor_460200_80133254(Task* task)
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

/// Ticks animation slots 1..0x13.
void func_actor_460200_801332E0(Task* task)
{
    Actor460200PairWork* work;
    s32                  i;

    work = (Actor460200PairWork*)task->work;
    i    = 1;
    do {
        Gp_AnimTickIndex(&work->anim, i);
        i++;
    } while (i < 0x14);
}

void func_actor_460200_8013332C(Task* task)
{
    ActorsShared80132514Work* work;
    s32                       i;

    work = (ActorsShared80132514Work*)task->work;
    i    = 1;
    do {
        work->slots[i].rate = 1;
        Gp_AnimResetSlot(&work->anim, i, work->field_4B8);
        i++;
    } while (i < 0x14);
    work->field_4B6 = work->field_4B8;
}

/// Reseeds animation slots 1..0x13 with clip `animId` and argument `animArg`,
/// and records the clip as the applied one.
void func_actor_460200_801333A4(Task* task)
{
    Actor460200PairWork* work;
    s32                  i;

    work = (Actor460200PairWork*)task->work;
    i    = 1;
    do {
        func_800B4114(&work->anim, i, work->animId, 0, work->animArg);
        i++;
    } while (i < 0x14);
    work->appliedAnimId = work->animId;
}

/// Script opcode: start animation `args->animId` on this actor, rejecting ids
/// of 0xC and above. State 1 (via `func_actor_460200_801333A4`) carries
/// `args->animArg`; state 2 (via `func_actor_460200_8013332C`) does not.
///
/// Twin of `func_actor_460200_80132B2C` in `actor_460200.c`; the `SOFT_BARRIER`
/// is the same codegen pin - without it the delay slot of the `beqz` fills from
/// the fall-through arm (`state = 1`) instead of the else arm's `state = 2`.
s32 func_actor_460200_80133408(Task* task, s32 arg1, Actor460200AnimArgs* args)
{
    Actor460200Work* work;

    work = (Actor460200Work*)task->work;
    if (args->animId >= 0xC) {
        return -1;
    }

    work->animId = args->animId;
    if (args->withArg != 0) {
        SOFT_BARRIER();
        work->state   = 1;
        work->animArg = args->animArg;
    } else {
        work->state = 2;
    }
    work->field_4BA = 0;
    func_actor_460200_80132F0C(task);
    return 0;
}
