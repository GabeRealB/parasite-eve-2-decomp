#include "common.h"

#include "actors/actor_323000.h"
#include "actors/actors_shared_80164af0.h"

/// Handler for message 0x7D3: latches the requested animation id into
/// `field_82E` and restarts the state machine at state 1.
s32 func_actor_323000_80164AF0(Task* task, s32 arg1, ActorShared80164af0Msg* msg, s32 arg3)
{
    Actor323000Work* work = (Actor323000Work*)task->work;

    work->field_82E = msg->field_4;
    work->field_0   = 1;
    work->field_2   = -1;
    return 0;
}
