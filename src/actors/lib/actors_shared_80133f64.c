#include "common.h"

#include "actors/actors_shared_80133f64.h"

#include "gameplay/1BC.h"

void ActorsShared80133f64(Task* task)
{
    ActorsShared80133f64Work* work;
    s32                       i;

    work            = (ActorsShared80133f64Work*)task->work;
    work->field_7BE = 0x30;
    work->field_7C0 = 0x800;
    i               = 1;
    do {
        work->slots0[i].rate = work->field_7BE;
        Gp_AnimResetSlot(&work->anim1, i, work->field_7BC);
        i++;
    } while (i < 8);
    i = 1;
    do {
        work->slots2[i].rate = work->field_7BE;
        Gp_AnimResetSlot(&work->anim3, i, work->field_7BC);
        i++;
    } while (i < 4);
    i = 1;
    do {
        work->slots4[i].rate = work->field_7BE;
        Gp_AnimResetSlot(&work->anim5, i, work->field_7BC);
        i++;
    } while (i < 4);
}
