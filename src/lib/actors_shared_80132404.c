#include "common.h"
#include "actors/actors_shared_80132404.h"
#include "gameplay/1BC.h"
#include "actors/actors_shared_80132514.h"

void ActorsShared80132404(Task* task)
{
    ActorsShared80132404Work* work;
    s32                       i;

    work = (ActorsShared80132404Work*)task->work;
    i    = 1;
    do {
        Gp_AnimTickIndex(&work->anim, i);
        i++;
    } while (i < 0x14);
}

void ActorsShared80132514(Task* task)
{
    ActorsShared80132514Work* work;
    s32                       i;

    work = (ActorsShared80132514Work*)task->work;
    i    = 1;
    do {
        work->slots[i].rate = 1;
        Gp_AnimResetSlot(&work->anim, i, work->field_4B8);
        i++;
    } while (i < 0x14);
    work->field_4B6 = work->field_4B8;
}
