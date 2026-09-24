#include "common.h"

#include "actors/actor_202600.h"
#include "gameplay/3A34.h"

/// Behaviour state 7. On entry it starts animation 0xE and stops the forward
/// and turn steps; each frame after that `Gp_TickObjFlag2` is ticked on the
/// context, and when it returns non-zero the actor goes to state 3
/// with animation 0xB, `field_3D2` cleared and a random 0..15 in `field_39E`.
void Actor02600_Fn0385C(Actor202600* arg0)
{
    Actor202600Work* work;
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
