#include "common.h"

#include "main/task.h"

#include "actors/actors_shared_801328cc.h"

void ActorsShared801328cc(Task* task)
{
    TaskFuncTable4 states;

    states = ActorsShared801328ccTable;
    states.funcs[task->state](task);
}
