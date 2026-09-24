#include "common.h"

#include "main/task.h"
#include "actors/actor_206100.h"

void func_actor_206100_8014F134(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_206100_80149E24;
    sp.funcs[task->state](task);
}
