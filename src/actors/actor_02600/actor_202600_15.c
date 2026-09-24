#include "common.h"

#include "actors/actor_202600.h"
#include "gameplay/gameplay.h"

extern u8 D_801153F4;

/// Per-frame tick, selected by the global mode `D_801153F4`. Mode 1 only
/// updates the colour and the ground shadow; mode 2 sets the model's `field_C` to
/// 0x80 and the context's `field_14` to 1 and stops there; mode 0 clears both
/// and then runs the full tick like any other mode. The full tick applies the
/// timed status damage when the context flags ask for it, resolves the
/// collision records, runs the behaviour state, the effect step while
/// `field_3B0` is set and the turn step while `field_3A6` is, moves and
/// animates the actor and refreshes its coordinate.
void Actor02600_Fn03558(Actor202600Ctx* arg0, Actor202600* arg1)
{
    s32               state;
    Actor202600Obj2C* obj;
    Actor202600Work*  work;
    GsCOORDINATE2*    coord;

    obj   = arg1->field_2C;
    state = D_801153F4;
    work  = arg1->field_1C;
    coord = obj->field_8;
    if (state == 1) {
        goto case1;
    }
    if (state >= 2) {
        goto ge2;
    }
    if (state == 0) {
        goto case0;
    }
    goto default_body;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto default_body;
case0:
    obj->field_C   = 0;
    arg0->field_14 = 0;
    goto default_body;
case2:
    obj->field_C   = 0x80;
    arg0->field_14 = 1;
    return;
default_body:
    if (arg0->field_4C != 0) {
        Actor02600_Fn0366C(arg1);
    }
    Actor02600_Fn0006C(arg1);
    Actor02600_Fn03784(arg1);
    if (work->field_3B0 != 0) {
        Actor02600_Fn020D4(arg1);
    }
    if (work->field_3A6 != 0) {
        Actor02600_Fn02214(arg1);
    }
    Actor02600_Fn03910(arg1);
    Actor02600_Fn039A4(arg1);
    coord->flg = 0;
    Gp_UpdateCoord(coord);
case1:
    Actor02600_Fn03A68(arg1);
    Actor02600_Fn03AC0(arg1);
}
