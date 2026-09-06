#include "common.h"

#include "actors/actors_shared_80164af0.h"

s32 ActorsShared80164af0(Task* task, s32 arg1, ActorShared80164af0Msg* msg, s32 arg3)
{
    ActorShared80164af0Work* work = (ActorShared80164af0Work*)task->idMap;

    work->field_82E = msg->field_4;
    work->field_0   = 1;
    work->field_2   = -1;
    return 0;
}
