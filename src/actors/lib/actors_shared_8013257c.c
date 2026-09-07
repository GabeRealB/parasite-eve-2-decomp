#include "common.h"

#include "actors/actors_shared_8013257c.h"

#include "gameplay/1BC.h"

void ActorsShared8013257c(Task* task)
{
    ActorsShared8013257cWork* work;
    s32                       i;

    work = (ActorsShared8013257cWork*)task->idMap;
    i    = 1;
    do {
        Gp_AnimTickIndex(&work->anim, i);
        i++;
    } while (i < 0x13);
}
