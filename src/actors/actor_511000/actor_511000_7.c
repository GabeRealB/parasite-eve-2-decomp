#include "common.h"
#include "main/task.h"
#include "main/tmd.h"
#include "gameplay/gameplay.h"
#include "actors/actor_511000.h"

void func_actor_511000_80133F48(void* enemy, Task* task);
void func_actor_511000_80133F88(void* enemy, Task* task);

void func_actor_511000_80133EF4(Task* task)
{
    void (*fns[2])(void*, Task*) = { func_actor_511000_80133F48, func_actor_511000_80133F88 };

    fns[task->state](task->spawnArg2, task);
}
