#include "common.h"

#include "actors/actor_215100.h"
#include "actors/actors_shared_8014c874.h"

s32 ActorsShared801325f0(Task* task, s32 arg1, Actor215100AnimArgs* args)
{
    Actor215100Work* work;

    work = (Actor215100Work*)task->work;
    if (args->animId >= 0x19) {
        return -1;
    }

    work->animId = args->animId;
    if (args->withArg != 0) {
        SOFT_BARRIER();
        work->state   = 1;
        work->animArg = args->animArg;
    } else {
        work->state = 2;
    }
    work->field_4BA = 0;
    ActorsShared8014c874(task);
    return 0;
}
