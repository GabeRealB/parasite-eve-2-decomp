#include "common.h"

#include "main/task.h"

#include "actors/actor_342400.h"

/// Once the hit flags are set, marks the enemy busy (`field_438`), requests
/// animation 4 and advances the sub-state.
void func_actor_342400_8016A2FC(Task* arg0)
{
    Actor342400Work* work;
    Actor342400Work* work2;

    work = (Actor342400Work*)arg0->work;
    if ((func_actor_342400_8016974C(arg0) << 0x10) != 0) {
        work->field_438  = 1;
        work->field_412  = 0;
        work2            = (Actor342400Work*)arg0->work;
        work2->field_426 = 4;
        work2->field_41C = 0x10;
        work2->field_418 = 4;
        work2->field_414 = 1;
        work->field_422  = work->field_422 + 1;
    }
}
