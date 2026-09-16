#include "common.h"

#include "actors/actor_350500.h"

#include "main/task.h"
#include "main/tmd.h"

INCLUDE_ASM("actors/nonmatchings/actor_350500/actor_350500_4", func_actor_350500_801629DC);

/// Message handler: latches the variant the payload's halfword at 0x2 selects
/// into `Actor350500Work::field_4C4` -- 0 for variant 1, 1 for variant 2 -- and
/// leaves the field alone for anything else. Returns 0 either way. The same
/// body as `func_actor_141000_80133F6C` and `func_actor_350700_80162AF4`.
s32 func_actor_350500_80162ABC(Task* task, s32 arg1, Actor350500Msg* msg)
{
    Actor350500Work* work;

    work = (Actor350500Work*)task->idMap;
    switch (msg->field_2) {
        case 1:
            work->field_4C4 = 0;
            break;
        case 2:
            work->field_4C4 = 1;
            break;
    }
    return 0;
}
