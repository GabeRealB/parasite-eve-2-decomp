#include "common.h"

#include "actors/actors_shared_80132c8c.h"

s32 ActorsShared80132c8c(ActorShared80132b88* arg0, s32 arg1, ActorShared80132b88Cmd* arg2)
{
    ActorShared80132b88Work* work;
    u16                      angle;

    angle = arg2->field_2;
    work  = arg0->field_1C;
    if (angle == 1) {
        work->field_4EE = angle;
    }
    return 0;
}
