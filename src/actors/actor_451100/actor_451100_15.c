#include "common.h"

#include "actors/actor_451100.h"
#include "main/task.h"

/// Message 0x7DB handler of `D_actor_451100_8013F704`: a zero payload
/// halfword sets the published block's `animArg` to 0x14, the count of frames
/// the step routine turns the model while clip 3 plays.
s32 func_actor_451100_8013268C(Task* task, s32 arg1, Actor451100Msg* msg)
{
    if (msg->field_2 == 0) {
        D_actor_451100_8014E744->animArg = 0x14;
    }
    return 0;
}
