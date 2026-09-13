#include "common.h"

#include "actors/actor_460200.h"
#include "actors/actors_shared_80132514.h"
#include "gameplay/1BC.h"
#include "main/task.h"

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200_2", func_actor_460200_80132CAC);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200_2", func_actor_460200_80132D74);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200_2", func_actor_460200_80132F0C);

void func_actor_460200_80132D74(void* enemy, Task* task);
void func_actor_460200_8013311C(void* enemy, Task* task);

void func_actor_460200_801330C8(Task* task)
{
    void (*fns[2])(void*, Task*) = { func_actor_460200_80132D74, func_actor_460200_8013311C };

    fns[task->state](task->spawnArg2, task);
}

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200_2", func_actor_460200_8013311C);

void func_actor_460200_8013322C(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200_2", func_actor_460200_80133254);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200_2", func_actor_460200_801332E0);

void func_actor_460200_8013332C(Task* task)
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

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200_2", func_actor_460200_801333A4);

void func_actor_460200_80132F0C(Task* task);

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

    work = (Actor460200Work*)task->idMap;
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

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200_2", func_actor_460200_80133474);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200_2", func_actor_460200_801334F0);
