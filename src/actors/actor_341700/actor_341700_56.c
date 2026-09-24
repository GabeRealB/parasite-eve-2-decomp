#include "common.h"

#include "main/task.h"

#include "actors/actor_341700.h"

/// Once the hit flags are set, returns the state machine to state 0.
void func_actor_341700_8016A08C(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;
    s32              cond;

    work = (Actor341700Work*)arg0->work;
    if ((work->flags_EC.half & 1) || (work->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work2            = (Actor341700Work*)arg0->work;
        work2->field_420 = 0;
        work2->field_422 = 0;
    }
}
