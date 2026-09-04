#include "common.h"
#include "main/task.h"
#include "actors/actor_342400.h"

void func_actor_342400_8016BBD0(void)
{
}

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_3", func_actor_342400_8016BBD8);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_3", func_actor_342400_8016BC70);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_3", func_actor_342400_8016BD3C);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_3", func_actor_342400_8016BD98);

void func_actor_342400_8016BED8(Task* arg0)
{
    Actor342400Work* work;

    work            = (Actor342400Work*)arg0->idMap;
    arg0->state     = 5;
    work->field_420 = 0;
    work->field_422 = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_3", func_actor_342400_8016BEF0);
