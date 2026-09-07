#include "common.h"

#include "actors/actors_shared_80132404.h"

#include "gameplay/1BC.h"

void ActorsShared80132404(Task* task)
{
    ActorsShared80132404Work* work;
    s32                       i;

    work = (ActorsShared80132404Work*)task->idMap;
    i    = 1;
    do {
        Gp_AnimTickIndex(&work->anim, i);
        i++;
    } while (i < 0x14);
}
