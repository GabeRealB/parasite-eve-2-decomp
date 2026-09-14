#include "common.h"

#include "actors/actor_205200.h"

s32 func_actor_205200_8014C9A0(Actor205200* arg0, s32 arg1, Actor205200Msg7DB* arg2)
{
    Actor205200Work* work;

    work = arg0->field_1C;
    if (arg2->field_2 != 0) {
        work->field_594 = 1;
    }
    return 0;
}
