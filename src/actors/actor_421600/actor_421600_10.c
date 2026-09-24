#include "common.h"

#include "actors/actor_421600.h"
#include "actors/actors_shared_80164af0.h"
#include "main/task.h"

/// Handler for message 0x7D3: latch the requested animation id into
/// `field_82E` and restart the state machine at state 1.
s32 func_actor_421600_8013E62C(Task* task, s32 arg1, ActorShared80164af0Msg* msg, s32 arg3)
{
    Actor421600Work* work = (Actor421600Work*)task->work;

    work->field_82E = msg->field_4;
    work->field_0   = 1;
    work->field_2   = -1;
    return 0;
}
