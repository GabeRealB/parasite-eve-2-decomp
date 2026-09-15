#include "common.h"

#include "actors/actor_511000.h"

#include "main/task.h"

INCLUDE_ASM("actors/nonmatchings/actor_511000/actor_511000_3", func_actor_511000_801327A0);

INCLUDE_ASM("actors/nonmatchings/actor_511000/actor_511000_3", func_actor_511000_8013287C);

INCLUDE_ASM("actors/nonmatchings/actor_511000/actor_511000_3", func_actor_511000_80132904);

INCLUDE_ASM("actors/nonmatchings/actor_511000/actor_511000_3", func_actor_511000_801329C4);

INCLUDE_ASM("actors/nonmatchings/actor_511000/actor_511000_3", func_actor_511000_80132B14);

INCLUDE_ASM("actors/nonmatchings/actor_511000/actor_511000_3", func_actor_511000_80132E6C);

INCLUDE_ASM("actors/nonmatchings/actor_511000/actor_511000_3", func_actor_511000_80133034);

INCLUDE_ASM("actors/nonmatchings/actor_511000/actor_511000_3", func_actor_511000_801330F0);

void func_actor_511000_80133220(Task* task)
{
    Task_Kill(task);
}

INCLUDE_ASM("actors/nonmatchings/actor_511000/actor_511000_3", func_actor_511000_80133240);

INCLUDE_ASM("actors/nonmatchings/actor_511000/actor_511000_3", func_actor_511000_801332E4);

void func_actor_511000_801333A4(Task* task)
{
    Task_Kill(task);
}

INCLUDE_ASM("actors/nonmatchings/actor_511000/actor_511000_3", func_actor_511000_801333C4);

void func_actor_511000_80133498(Task* task)
{
    Task_Kill(task);
}

s32 func_actor_511000_801334B8(Task* arg0)
{
    arg0->killCountdown = 0;
    return 0;
}
