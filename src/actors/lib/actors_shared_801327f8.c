#include "common.h"

#include "actors/actors_shared_801327f8.h"

void ActorsShared801327f8(Task* task)
{
    ActorsShared801327f8Work* work;
    TaskFuncTable4            fns;

    work = (ActorsShared801327f8Work*)task->idMap;
    fns  = ActorsShared801327f8Table;
    fns.funcs[(s16)work->turnCount](task);
}
