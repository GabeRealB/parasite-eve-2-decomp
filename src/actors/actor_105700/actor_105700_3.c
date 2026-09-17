#include "common.h"

#include <psyq/inline_c.h>

#include "actors/actor_105700.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

extern s8 D_80115419;

/// Per-frame tick, the same body as `Actor02000_Fn03268` of `actor_102000`.
/// State 0 counts `field_6AE` up to 0x5B frames and then hands over to state
/// 1 with animation 4, running `func_actor_105700_80132B28` every frame
/// meanwhile; state 1 waits for `field_698` to reach 0x5E and drops back to
/// state 0 with animation 1. Either way, once `field_6B2` or the global
/// `D_80115419` is set the actor switches to animation 2 and arms the shared
/// state-F0 slot.
void func_actor_105700_80136AE0(Actor105700* arg0)
{
    Actor105700Work* work;
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
            func_actor_105700_80132B28(arg0);
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

/// State advance for the "handover" clip, the same body as
/// `Actor02000_Fn03348` of `actor_102000`. State 0 arms animation 0x11 and
/// clears the pair of dwell counters; state 1 waits for `field_698` to reach
/// 0x37 and then picks animation 2 with a 2-frame park in `field_6A6`, or
/// animation 0x14 with a 10-frame park when `field_6E0` is set. Either path
/// drops back to state 0.
void func_actor_105700_80136BC0(Actor105700* arg0)
{
    Actor105700Work* work;
    s16              state;

    work  = arg0->field_1C;
    state = work->field_6A8;
    switch (state) {
        case 0:
            work->field_694 = 0x11;
            work->field_6A8 = 1;
            work->field_69C = 0;
            work->field_69E = 0;
            break;
        case 1:
            if (work->field_698 >= 0x37) {
                if (work->field_6E0 == 0) {
                    work->field_694 = 2;
                    work->field_6A6 = 2;
                    work->field_6A8 = 0;
                } else {
                    work->field_694 = 0x14;
                    work->field_6A6 = 0xA;
                    work->field_6A8 = 0;
                }
            }
            break;
    }
}
