#include "common.h"

#include "main/task.h"
#include "rooms/room_common.h"

/// Cap script state 1: sets the first action prompt's `targetId` to 0x80 and its
/// `mode` to 1, zeroes its on-screen position, and steps the script on.
void func_neo_ark_shrine_8017EDAC(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;

    prompt->targetId    = 0x80;
    prompt->mode        = 1;
    prompt->screen.xy.x = 0;
    prompt->screen.xy.y = 0;
    task->state         = task->state + 1;
}
