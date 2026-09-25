#include "common.h"

#include "actors/actor_105600.h"
#include "gameplay/3A34.h"
#include "main/task.h"

/// Nonzero sends the idle handlers to handler 2 with animation 2; the
/// teardown raises it.
extern s8 D_80115419;

void Actor05600_Fn00CFC(Task* arg0);

/// Entry 0 of `Actor05600_D16540`: state 0 counts `field_6AE` up to 0x5B
/// frames, running the proximity check meanwhile, then switches to animation
/// 4 and state 1; state 1 waits for frame 0x5E and returns to animation 1 and
/// state 0. A set `field_6B2` or `D_80115419` overrides everything with
/// animation 2, handler 2 and the shared state-F0 slot.
void Actor05600_Fn046F0(Task* arg0)
{
    Actor105600Work* work;
    s16              state;

    work  = (Actor105600Work*)arg0->work;
    state = work->field_6A8;
    switch (state) {
        case 0:
            work->field_6AE++;
            if (work->field_6AE >= 0x5B) {
                work->field_694 = 4;
                work->field_6AE = 0;
                work->field_6A8 = 1;
            }
            Actor05600_Fn00CFC(arg0);
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
