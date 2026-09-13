#include "common.h"

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

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200_2", func_actor_460200_80133408);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200_2", func_actor_460200_80133474);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200_2", func_actor_460200_801334F0);
