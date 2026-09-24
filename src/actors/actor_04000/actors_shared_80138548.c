#include "common.h"

#include "actors/actor_104000.h"
#include "actors/actors_shared_80138548.h"
#include "main/task.h"

/// Handler for message 0x7D3: latches the animation id the sender asks for and
/// picks motion state 2 when its flag is clear, 1 otherwise. Always answers 1.
s32 Actor04000_Fn06728(Task* task, s32 arg1, ActorShared80138548Msg* msg, s32 arg3)
{
    Actor104000Work* work = (Actor104000Work*)task->work;

    work->field_174 = msg->field_4;
    if (msg->field_8 == 0) {
        work->field_170 = 2;
    } else {
        work->field_170 = 1;
    }
    return 1;
}
