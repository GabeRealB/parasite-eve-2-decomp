#include "common.h"

#include "main/task.h"
#include "rooms/room_common.h"

/// Sets the first action prompt to mode 1 with target id 0x80, clears its
/// screen position, and steps the task on one state.
void func_dryfield_night_motel_lobby_80180FA4(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;

    prompt->targetId    = 0x80;
    prompt->mode        = 1;
    prompt->screen.xy.x = 0;
    prompt->screen.xy.y = 0;
    task->state         = task->state + 1;
}
