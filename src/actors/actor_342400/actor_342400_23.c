#include "common.h"
#include "main/task.h"
#include "actors/actor_342400.h"

void func_actor_342400_8016BED8(Task* arg0)
{
    Actor342400Work* work;

    work            = (Actor342400Work*)arg0->idMap;
    arg0->state     = 5;
    work->field_420 = 0;
    work->field_422 = 0;
}
