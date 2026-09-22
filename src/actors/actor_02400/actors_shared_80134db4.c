#include "common.h"

#include "actors/actors_shared_80134db4.h"

extern u32 Gp_LcgState;

void ActorsShared80134db4(ActorShared80134db4* arg0)
{
    ActorShared80134db4Work*    work;
    ActorShared80134db4Obj130** detach;
    u16                         sweep;
    u16                         counter;
    u32                         state;

    work = arg0->field_1C;
    if (work->field_142 == 0) {
        sweep           = work->field_12A + 0x200;
        work->field_12A = sweep;
        if ((s16)sweep >= 0x1801) {
            work->field_142 = 1;
        }
    } else {
        sweep           = work->field_12A - 0x200;
        work->field_12A = sweep;
        if ((s16)sweep < 0x1400) {
            work->field_142 = 0;
        }
    }
    detach = work->field_130;
    if (detach != NULL) {
        (*detach)->field_30 = 4;
        work->field_130     = NULL;
    }
    work->field_DE  = work->field_DE & 0x7FFF;
    counter         = work->field_140 + 1;
    work->field_140 = counter;
    if ((s16)counter >= 0x10) {
        state           = (Gp_LcgState * 5) + 0x71357911;
        work->field_13C = 1;
        work->field_13E = 0;
        Gp_LcgState     = state;
        work->field_140 = (u16)(((state >> 0x10) & 0x1F) + 0x1E);
        work->field_DE  = work->field_DE | 0x8000;
    }
}
