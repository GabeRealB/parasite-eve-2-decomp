#include "common.h"

#include "gameplay/3A34.h"

#include "actors/actor_102300.h"

extern s8 D_80115419;

void Actor02300_Fn00CD0(Actor102300* arg0);

/// Entry 0 of the `field_6A6` state table, the idle. State 0 counts
/// `field_6AE` up to 0x5B frames and then switches to animation 4 and state 1,
/// running the proximity check `Actor02300_Fn00CD0` every frame meanwhile;
/// state 1 waits for `field_698` to reach 0x5E and drops back to state 0 with
/// animation 1. A set `field_6B2` or `D_80115419` overrides both with
/// animation 2, entry 2 and the shared state-F0 slot.
void Actor02300_Fn03828(Actor102300* arg0)
{
    Actor102300Work* work;
    s16              state;

    work  = arg0->field_1C;
    state = work->field_6A8;
    switch (state) {
        case 0:
            work->field_6AE++;
            if (work->field_6AE >= 0x5B) {
                work->field_694 = 4;
                work->field_6AE = 0;
                work->field_6A8 = 1;
            }
            Actor02300_Fn00CD0(arg0);
            break;
        case 1:
            if (work->field_698 >= 0x5E) {
                work->field_694 = 1;
                work->field_6A8 = 0;
            }
            break;
    }

    if ((work->field_6B2 != 0) || (D_80115419 != 0)) {
        work->field_6A6 = 2;
        work->field_6A8 = 0;
        work->field_694 = 2;
        work->field_6AE = 0;
        Gp_ArmStateF0(1);
    }
}
