#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

#include "actors/actor_342000.h"

void func_actor_342000_80163F88(Task* task)
{
    Actor342000Work* work;
    GsCOORDINATE2*   coord;

    coord = (GsCOORDINATE2*)((TmdObject*)task->extra)->field_8;
    work  = (Actor342000Work*)task->work;

    coord->sub = work->field_2A4;
    Task_Kill(task);
}
