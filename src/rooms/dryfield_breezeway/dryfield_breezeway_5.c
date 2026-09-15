#include "common.h"

#include <psyq/libgte.h>

#include "rooms/room_common.h"

/// The action-prompt arm `Room_Util14` performs, plus a reset of the caller's
/// kill countdown: highlights the prompt for the fixed target id 0x80, clears
/// the on-screen position `func_800D4E78` fills in again when the prompt is
/// spawned, and steps the caller's script on one state.
void func_dryfield_breezeway_8017FD68(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;

    prompt->targetId    = 0x80;
    prompt->mode        = 1;
    prompt->screen.xy.x = 0;
    prompt->screen.xy.y = 0;
    task->killCountdown = 0;
    task->state         = task->state + 1;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway_5", func_dryfield_breezeway_8017FD9C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway_5", func_dryfield_breezeway_8017FE08);
