#include "common.h"

#include "actors/actor_105500.h"
#include "gameplay/3A34.h"

extern u32 Gp_LcgState;

void ActorsShared801355a4_Fn3567C(Actor105500* arg0)
{
    Actor105500Work* work;
    s16              state;
    u32              random;

    work  = arg0->field_1C;
    state = work->field_39C;
    switch (state) {
        case 0:
            work->field_392 = 0xE;
            work->field_398 = 0;
            work->field_3A6 = 0;
            work->field_39C = 1;
            return;
        case 1:
            if (Gp_TickObjFlag2((GpObj5D*)arg0->field_20) != 0) {
                work->field_39A = 3;
                work->field_39C = 0;
                work->field_392 = 0xB;
                work->field_3D2 = 0;
                random          = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState     = random;
                work->field_39E = (s16)((random >> 0x10) & 0xF);
            }
            return;
    }
}
