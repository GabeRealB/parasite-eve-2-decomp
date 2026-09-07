#include "common.h"
#include "main/task.h"

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800_3", func_actor_450800_80132CE0);

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800_3", func_actor_450800_80132D74);

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800_3", func_actor_450800_80132E9C);

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800_3", func_actor_450800_801330AC);

void func_actor_450800_80132E9C(void* enemy, Task* task);
void func_actor_450800_801332B8(void* enemy, Task* task);

void func_actor_450800_80133264(Task* task)
{
    void (*fns[2])(void*, Task*) = { func_actor_450800_80132E9C, func_actor_450800_801332B8 };

    fns[task->state](task->spawnArg2, task);
}

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800_3", func_actor_450800_801332B8);
