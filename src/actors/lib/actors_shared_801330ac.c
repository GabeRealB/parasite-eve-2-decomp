#include "common.h"

#include "actors/actors_shared_801330ac.h"

#include "actors/actors_shared_8013257c.h"
#include "actors/actors_shared_801325c8.h"
#include "actors/actors_shared_80132640.h"
#include "actors/actors_shared_8014c874.h"
#include "main/tmd.h"

void ActorsShared801330ac(Task* task)
{
    ActorsShared801330acWork* work;
    s16                       animId;

    work = (ActorsShared801330acWork*)task->idMap;
    if (work->state == 1) {
        ActorsShared80132640(task);
        work->state = 3;
        return;
    }
    if (work->state == 2) {
        ActorsShared801325c8(task);
        work->state = 3;
        return;
    }
    if (work->state == 3) {
        do {
        } while (0);
        animId = work->animId;
        if (animId == 4 && work->travel != 0) {
            ActorsShared8014c874_MoveForward(((TmdObject*)task->extra)->field_8, 0xC);
            work->travel = (u16)work->travel - 1;
            if (work->travel == 0) {
                work->animArg = 0xA;
                work->animId  = 1;
            }
        }
        ActorsShared8013257c(task);
        return;
    }
}
