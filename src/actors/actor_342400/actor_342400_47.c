#include "common.h"

#include "main/task.h"

#include "gameplay/3A34.h"

#include "actors/actor_342400.h"

/// Once the hit flags are set, requests animation 0xB; once the enemy's
/// flag-2 counter runs out, moves the state machine to state 3.
void func_actor_342400_80169D2C(Task* arg0)
{
    Actor342400Work* work;
    Actor342400Work* work2;

    if ((func_actor_342400_8016974C(arg0) << 0x10) != 0) {
        work            = (Actor342400Work*)arg0->work;
        work->field_426 = 4;
        work->field_41C = 0x10;
        work->field_418 = 0xB;
        work->field_414 = 1;
    }
    if (Gp_TickObjFlag2((GpObj5D*)arg0->spawnArg2) != 0) {
        work2            = (Actor342400Work*)arg0->work;
        work2->field_420 = 3;
        work2->field_422 = 0;
    }
}
