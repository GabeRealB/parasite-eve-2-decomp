#include "common.h"
#include "main/task.h"
#include "main/tmd.h"
#include "actors/actor_342400.h"

/// Advances the sub-state once the frame counter has passed 0x50.
void func_actor_342400_8016A240(Task* arg0)
{
    u16              ticks;
    Actor342400Work* work;

    work            = (Actor342400Work*)arg0->work;
    ticks           = work->field_412;
    work->field_412 = ticks + 1;
    if ((s16)ticks >= 0x51) {
        work->field_422 = work->field_422 + 1;
    }
}

/// Once the hit flags are set, releases this enemy's `Gp_StateF0` hold,
/// requests animation 0xF and advances the sub-state.
void func_actor_342400_8016A280(Task* arg0)
{
    Actor342400Work* work;
    Actor342400Work* work2;

    work = (Actor342400Work*)arg0->work;
    if ((func_actor_342400_8016974C(arg0) << 0x10) != 0) {
        func_actor_342400_801694A8(arg0, 0);
        work2            = (Actor342400Work*)arg0->work;
        work2->field_426 = 8;
        work2->field_41C = 0x10;
        work2->field_418 = 0xF;
        work2->field_414 = 1;
        work->field_422  = work->field_422 + 1;
    }
}
