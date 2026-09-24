#include "common.h"

#include "main/task.h"

#include "actors/actor_341700.h"

/// Once the hit flags are set, marks the enemy busy (`field_438`), requests
/// animation 4 and advances the sub-state.
void func_actor_341700_80169018(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;

    work = (Actor341700Work*)arg0->work;
    if ((func_actor_341700_80168468(arg0) << 0x10) != 0) {
        work->field_438  = 1;
        work->field_412  = 0;
        work2            = (Actor341700Work*)arg0->work;
        work2->field_426 = 4;
        work2->field_41C = 0x10;
        work2->field_418 = 4;
        work2->field_414 = 1;
        work->field_422  = work->field_422 + 1;
    }
}
