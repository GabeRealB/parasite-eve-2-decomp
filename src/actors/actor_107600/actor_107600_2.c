#include "common.h"

#include "main/mem.h"
#include "main/gfx.h"
#include "main/task.h"

#include "gameplay/1BC.h"
#include "gameplay/3FB8.h"

#include "actors/actor_107600.h"

/// Applies the transition `work->field_15E` queues once `field_156` is 1:
/// requests 1..5 open states 2, 3, 4, 6 and 5 through the same stores as
/// `func_actor_107600_80134B98` (written out, since the setter is not
/// inlined). The request is always consumed; returns whether one was pending.
s32 func_actor_107600_80134BAC(Actor107600* arg0)
{
    Actor107600Work* work = arg0->field_1C;

    if (work->field_156 == 1) {
        switch ((s16)(work->field_15E - 1)) {
            case 0: {
                Actor107600Work* w = arg0->field_1C;

                w->field_158 = 2;
                w->field_15A = 0;
                break;
            }
            case 1: {
                Actor107600Work* w = arg0->field_1C;

                w->field_158 = 3;
                w->field_15A = 0;
                break;
            }
            case 2: {
                Actor107600Work* w = arg0->field_1C;

                w->field_158 = 4;
                w->field_15A = 0;
                break;
            }
            case 3: {
                Actor107600Work* w = arg0->field_1C;

                w->field_158 = 6;
                w->field_15A = 0;
                break;
            }
            case 4: {
                Actor107600Work* w = arg0->field_1C;

                w->field_158 = 5;
                w->field_15A = 0;
                break;
            }
        }
        work->field_15E = 0;
        return 1;
    }
    return 0;
}
