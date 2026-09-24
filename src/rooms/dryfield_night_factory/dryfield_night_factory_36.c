#include "common.h"

#include "main/task.h"
#include "rooms/room_common.h"

/// Script state: highlights the action prompt (`mode` 1, target id 0x80),
/// clears its screen position and steps the script on one state.
void func_dryfield_night_factory_80181938(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;

    prompt->targetId    = 0x80;
    prompt->mode        = 1;
    prompt->screen.xy.x = 0;
    prompt->screen.xy.y = 0;
    task->state         = task->state + 1;
}
