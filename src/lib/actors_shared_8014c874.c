#include "common.h"

#include "actors/actors_shared_8014c874.h"

#include "actors/actors_shared_80132404.h"
#include "actors/actors_shared_801324c8.h"
#include "actors/actors_shared_80132514.h"
#include "main/tmd.h"

void ActorsShared8014c874(Task* task)
{
    ActorsShared8014c874Work* work;
    s16                       animId;

    work = (ActorsShared8014c874Work*)task->work;
    if (work->state == 1) {
        ActorsShared801324c8(task);
        work->state = 3;
        return;
    }
    if (work->state == 2) {
        ActorsShared80132514(task);
        work->state = 3;
        return;
    }
    if (work->state == 3) {
        // The loop-end note ends cse's first block here, so the pause check
        // loads its own 1 instead of reusing the state test's.
        do {
        } while (0);
        animId = work->animId;
        if (animId == 4 && work->travel != 0) {
            ActorsShared8014c874_MoveForward(((TmdObject*)task->extra)->coords, 0xC);
            work->travel = (u16)work->travel - 1;
            if (work->travel == 0) {
                work->animArg = 0xA;
                work->animId  = 1;
            }
        }
        ActorsShared80132404(task);
        return;
    }
}
