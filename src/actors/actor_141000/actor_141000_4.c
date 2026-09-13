#include "common.h"

#include "actors/actor_141000.h"

#include "main/task.h"
#include "main/tmd.h"

INCLUDE_ASM("actors/nonmatchings/actor_141000/actor_141000_4", func_actor_141000_80133E8C);

s32 func_actor_141000_80133F6C(Task* task, s32 arg1, Actor141000Msg* msg)
{
    Actor141000Work* work;

    work = (Actor141000Work*)task->idMap;
    switch (msg->field_2) {
        case 1:
            work->field_4C8 = 0;
            break;
        case 2:
            work->field_4C8 = 1;
            break;
    }
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_141000/actor_141000_4", func_actor_141000_80133FA8);
