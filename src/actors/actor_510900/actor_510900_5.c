#include "common.h"
#include "main/task.h"
#include "actors/actor_510900.h"

void func_actor_510900_8013A5B8(void* enemy, Task* task);
void func_actor_510900_8013A85C(void* enemy, Task* task);

void func_actor_510900_8013C1EC(Task* task)
{
    void (*fns[2])(void*, Task*) = { func_actor_510900_8013A5B8, func_actor_510900_8013A85C };

    fns[task->state](task->spawnArg2, task);
}

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900_5", func_actor_510900_8013C240);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900_5", func_actor_510900_8013C338);

void func_actor_510900_8013C380(Actor510900* arg0)
{
    GpEnemy*         enemy = arg0->field_20;
    Actor510900Work* work  = arg0->field_1C;

    Gp_UnlinkNode(&enemy->node);
    Gp_UnlinkObj(&work->obj2BC);
    Gp_UnlinkObj(&work->obj2F4);
    Gp_DestroyEnemy(enemy, (Task*)arg0);
}

void func_actor_510900_8013AD90(void* enemy, Task* task);
void func_actor_510900_8013AF38(void* enemy, Task* task);

void func_actor_510900_8013C3DC(Task* task)
{
    void (*fns[2])(void*, Task*) = { func_actor_510900_8013AD90, func_actor_510900_8013AF38 };

    fns[task->state](task->spawnArg2, task);
}

void func_actor_510900_8013C430(Actor510900* arg0)
{
    GpEnemy*         enemy = arg0->field_20;
    Actor510900Work* work  = arg0->field_1C;

    Gp_UnlinkNode(&enemy->node);
    Gp_UnlinkObj(&work->obj0);
    Gp_UnlinkObj(&work->obj38);
    Gp_DestroyEnemy(enemy, (Task*)arg0);
}
