#include "common.h"
#include "main/task.h"
#include "actors/actor_342400.h"

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_20", func_actor_342400_8016BD3C);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_20", func_actor_342400_8016BD98);

void func_actor_342400_8016BED8(Task* arg0)
{
    Actor342400Work* work;

    work            = (Actor342400Work*)arg0->idMap;
    arg0->state     = 5;
    work->field_420 = 0;
    work->field_422 = 0;
}
