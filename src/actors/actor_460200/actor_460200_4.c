#include "common.h"

#include "actors/actor_460200.h"
#include "actors/actors_shared_80132514.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/gameplay.h"
#include "main/task.h"
#include "main/tmd.h"

void func_actor_460200_801336B4(Task* task);
void func_actor_460200_80133AB0(Task* task);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200_4", func_actor_460200_8013364C);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200_4", func_actor_460200_801336B4);

void func_actor_460200_801338C0(void* enemy, Task* task);
void func_actor_460200_80133A04(void* enemy, Task* task);

void func_actor_460200_8013386C(Task* task)
{
    void (*fns[2])(void*, Task*) = { func_actor_460200_801338C0, func_actor_460200_80133A04 };

    fns[task->state](task->spawnArg2, task);
}

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200_4", func_actor_460200_801338C0);

void func_actor_460200_80133A04(void* arg0, Task* task)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    VECTOR         vec;

    obj   = task->extra;
    coord = obj->field_8;
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

    work = (ActorsShared80132514Work*)task->idMap;
    i    = 1;
    do {
        work->slots[i].field_9 = 1;
        Gp_AnimResetSlot(&work->anim, i, work->field_4B8);
        i++;
    } while (i < 0x14);
    work->field_4B6 = work->field_4B8;
}

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200_4", func_actor_460200_80133C00);

s32 func_actor_460200_80133C64(Task* task, s32 arg1, Actor460200AnimArgs* args)
{
    Actor460200Work* work;

    work = (Actor460200Work*)task->idMap;
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
/// models (`TmdObject::field_C` = 0) and its absence restores the default
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
    work = (Actor460200PairedWork*)task->idMap;
    if (task->spawnArg1 != 0) {
        other = (TmdObject*)work->field_4F0->extra;
    } else {
        other = self;
    }
    if (flags & 1) {
        self->field_C  = 0;
        other->field_C = 0;
    } else {
        self->field_C  = 0x80;
        other->field_C = 0x80;
    }
    if (flags & 2) {
        self->field_C  |= 4;
        other->field_C |= 4;
    }
    return 0;
}
