#include "common.h"

#include "main/task.h"

#include "gameplay/3A34.h"

#include "actors/actor_341700.h"

/// Once the hit flags are set, requests animation 0xB; once the enemy's
/// flag-2 counter runs out, moves the state machine to state 3.
void func_actor_341700_80168A48(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;

    if ((func_actor_341700_80168468(arg0) << 0x10) != 0) {
        work            = (Actor341700Work*)arg0->work;
        work->field_426 = 4;
        work->field_41C = 0x10;
        work->field_418 = 0xB;
        work->field_414 = 1;
    }
    if (Gp_TickObjFlag2((GpObj5D*)arg0->spawnArg2) != 0) {
        work2            = (Actor341700Work*)arg0->work;
        work2->field_420 = 3;
        work2->field_422 = 0;
    }
}
