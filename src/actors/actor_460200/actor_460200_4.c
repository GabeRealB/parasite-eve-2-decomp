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

extern s32 D_actor_460200_801514FC;
extern s32 D_actor_460200_80151538;

void func_actor_460200_801336B4(Task* task);
void func_actor_460200_80133A88(Task* task);
void func_actor_460200_80133AB0(Task* task);

/// Per-frame handler of one of this actor's sub-model tasks: the same body as
/// `ActorsShared801328bc`, reached with the sub-model's own `TmdObject` in
/// `Task::extra` and the actor holding it as `Task::parent`.
void func_actor_460200_8013364C(Task* task)
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

void func_actor_460200_80133B3C(Task* task);
void func_actor_460200_80133B88(Task* task);
void func_actor_460200_80133C00(Task* task);

/// The same step body as `ActorsShared8014c874`, calling this overlay's own
/// reseed and slot-tick routines.
void func_actor_460200_801336B4(Task* task)
{
    ActorsShared8014c874Work* work;
    s16                       animId;

    work = (ActorsShared8014c874Work*)task->work;
    if (work->state == 1) {
        func_actor_460200_80133C00(task);
        work->state = 3;
        return;
    }
    if (work->state == 2) {
        func_actor_460200_80133B88(task);
        work->state = 3;
        return;
    }
    if (work->state == 3) {
        // The loop-end note ends cse's first block here, so the pause check
        // loads its own 1 instead of reusing the state test's.
        do {
        } while (0);
        animId = work->animId;
        if (animId == 4 && work->travel != 0) {
            ActorsShared8014c874_MoveForward(((TmdObject*)task->extra)->coords, 0xC);
            work->travel = (u16)work->travel - 1;
            if (work->travel == 0) {
                work->animArg = 0xA;
                work->animId  = 1;
            }
        }
        func_actor_460200_80133B3C(task);
        return;
    }
}

void func_actor_460200_801338C0(GpEnemy* enemy, Task* task);
void func_actor_460200_80133A04(void* enemy, Task* task);

void func_actor_460200_8013386C(Task* task)
{
    void (*fns[2])(void*, Task*) = { func_actor_460200_801338C0, func_actor_460200_80133A04 };

    fns[task->state](task->spawnArg2, task);
}

/// Spawn routine of the actor whose `func_actor_460200_80133A88` exit path
/// hands it back to `Gp_DestroyEnemy`: it allocates the 0x4F8 work block (the
/// matrix pair its sub-model reads through `TmdObject::lightMtx`/`field_20`
/// plus the animation state below), parks the enemy in `Actor460200Work::enemy`
/// and starts state 2, the visibility opcode `func_actor_460200_801325FC` runs.
void func_actor_460200_801338C0(GpEnemy* enemy, Task* task)
{
    Actor460200Work* work;
    void*            workMem;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    MATRIX*          mtx;
    VECTOR           vec;

    obj     = task->extra;
    coord   = obj->coords;
    workMem = memCalloc(0x4F8, 0);
    work    = (Actor460200Work*)workMem;
    if ((task->work = (TaskIdMap*)work) == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback   = func_actor_460200_80133A88;
    coord->sub           = &gGfxViewCoord;
    enemy->field_4       = &coord->coord;
    enemy->field_48      = 0;
    enemy->node.targeted = 0;
    enemy->node.flags    = 1;
    obj->flags           = 0;
    obj->otOffset        = 1;
    work->enemy          = enemy;
    work->animId         = 2;
    mtx                  = (MATRIX*)work;
    obj->lightMtx        = mtx;
    obj->colorMtx        = mtx + 1;
    vec.vx               = coord->workm.t[0];
    vec.vy               = coord->workm.t[1] - 0x320;
    vec.vz               = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&work->anim, &D_actor_460200_80151538, obj, &work->slots[0x14], work->slots);
    work->state    = 2;
    task->msgTable = &D_actor_460200_801514FC;
    func_actor_460200_801336B4(task);
    task->state += 1;
}

void func_actor_460200_80133A04(void* arg0, Task* task)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    VECTOR         vec;

    obj   = task->extra;
    coord = obj->coords;
    Gp_UpdateCoord(coord);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1] - 0x320;
    vec.vz = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_actor_460200_801336B4(task);
    func_actor_460200_80133AB0(task);
}

void func_actor_460200_80133A88(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200_4", func_actor_460200_80133AB0);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200_4", func_actor_460200_80133B3C);

void func_actor_460200_80133B88(Task* task)
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

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200_4", func_actor_460200_80133C00);

s32 func_actor_460200_80133C64(Task* task, s32 arg1, Actor460200AnimArgs* args)
{
    Actor460200Work* work;

    work = (Actor460200Work*)task->work;
    if (args->animId >= 0x12) {
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
    func_actor_460200_801336B4(task);
    return 0;
}

/// Script opcode: set the visibility flags of this actor's model and of the
/// model owned by the enemy task it was paired with. `flags` bit 0 hides both
/// models (`TmdObject::flags` = 0) and its absence restores the default
/// 0x80; bit 1 additionally ORs in 0x4. With no enemy paired
/// (`Task::spawnArg1` == 0) the actor drives its own model twice.
///
/// This is the byte-identical body of `func_actor_460200_80132B98`; the two
/// overlays each carry their own copy of it.
s32 func_actor_460200_80133CD0(Task* task, s32 arg1, s32 flags)
{
    Actor460200PairedWork* work;
    TmdObject*             self;
    TmdObject*             other;

    self = (TmdObject*)task->extra;
    work = (Actor460200PairedWork*)task->work;
    if (task->spawnArg1 != 0) {
        other = (TmdObject*)work->field_4F0->extra;
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
