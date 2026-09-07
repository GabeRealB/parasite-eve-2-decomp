#include "common.h"

#include "actors/actors_shared_80138548.h"

s32 ActorsShared80138548(Task* task, s32 arg1, ActorShared80138548Msg* msg, s32 arg3)
{
    ActorShared80138548Work* work = (ActorShared80138548Work*)task->idMap;

    work->field_174 = msg->field_4;
    if (msg->field_8 == 0) {
        work->field_170 = 2;
    } else {
        work->field_170 = 1;
    }
    return 1;
}
