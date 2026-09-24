#include "common.h"

#include "main/task.h"

#include "actors/actor_342400.h"

/// Once the hit flags are set, requests animation 7 (when `field_44F` is 1)
/// or 1, and advances the sub-state.
void func_actor_342400_80169E24(Task* arg0)
{
    Actor342400Work* work;
    Actor342400Work* slow;
    Actor342400Work* fast;

    work = (Actor342400Work*)arg0->work;
    if (func_actor_342400_8016974C(arg0) != 0) {
        if (work->field_44F == 1) {
            fast            = (Actor342400Work*)arg0->work;
            fast->field_426 = 0x32;
            fast->field_41C = 0x10;
            fast->field_418 = 7;
            fast->field_414 = 1;
        } else {
            slow            = (Actor342400Work*)arg0->work;
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
void func_actor_342400_80169EC4(Task* arg0)
{
    Actor342400Work* work = (Actor342400Work*)arg0->work;

    if (func_actor_342400_8016974C(arg0)) {
        if (work->field_44F == 1) {
            Actor342400Work* w = (Actor342400Work*)arg0->work;

            w->field_420 = 3;
            w->field_422 = 0;
        } else {
            Actor342400Work* w = (Actor342400Work*)arg0->work;

            w->field_420 = 5;
            w->field_422 = 0;
        }
    }
}
