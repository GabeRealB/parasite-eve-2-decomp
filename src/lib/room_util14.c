#include "common.h"

#include "main/task.h"
#include "rooms/room_common.h"

/// Arms the action prompt for a hotspot and steps the caller's script on one
/// state: marks the prompt as highlighted (`mode` 1) for the fixed target id
/// 0x80 and resets the on-screen position, which `func_800D4E78` fills in again
/// when the prompt is actually spawned.
///
/// Shared body, linked into every room overlay that uses it.
void Room_Util14(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;

    prompt->targetId    = 0x80;
    prompt->mode        = 1;
    prompt->screen.xy.x = 0;
    prompt->screen.xy.y = 0;
    task->state         = task->state + 1;
}
