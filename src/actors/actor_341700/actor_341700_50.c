#include "common.h"

#include "main/task.h"

#include "actors/actor_341700.h"

/// Once the hit flags are set, requests animation 7 (when `field_44F` is 1)
/// or 1, and advances the sub-state.
void func_actor_341700_80168B40(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* slow;
    Actor341700Work* fast;

    work = (Actor341700Work*)arg0->work;
    if (func_actor_341700_80168468(arg0) != 0) {
        if (work->field_44F == 1) {
            fast            = (Actor341700Work*)arg0->work;
            fast->field_426 = 0x32;
            fast->field_41C = 0x10;
            fast->field_418 = 7;
            fast->field_414 = 1;
        } else {
            slow            = (Actor341700Work*)arg0->work;
            slow->field_426 = 0x1E;
            slow->field_41C = 0x10;
            slow->field_418 = 1;
            slow->field_414 = 1;
        }
        work->field_422 = work->field_422 + 1;
    }
}

/// Once the hit flags are set, moves the state machine to state 3 (when
/// `field_44F` is 1) or 5.
void func_actor_341700_80168BE0(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->work;

    if (func_actor_341700_80168468(arg0)) {
        if (work->field_44F == 1) {
            Actor341700Work* w = (Actor341700Work*)arg0->work;

            w->field_420 = 3;
            w->field_422 = 0;
        } else {
            Actor341700Work* w = (Actor341700Work*)arg0->work;

            w->field_420 = 5;
            w->field_422 = 0;
        }
    }
}
