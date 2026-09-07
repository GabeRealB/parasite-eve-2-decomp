#include "common.h"

#include "actors/actors_shared_801325c8.h"

#include "gameplay/1BC.h"

void ActorsShared801325c8(Task* task)
{
    ActorsShared801325c8Work* work;
    s32                       i;

    work = (ActorsShared801325c8Work*)task->idMap;
    for (i = 1; i < 0x13; i++) {
        work->slots[i].field_9 = 1;
        Gp_AnimResetSlot(&work->anim, i, work->field_480);
    }
    work->field_47E = work->field_480;
}
