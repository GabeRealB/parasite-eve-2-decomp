#include "common.h"

#include "main/task.h"
#include "rooms/room_common.h"

/// Resets both action prompts - cursor position cleared, target id 0x100,
/// `field_E` 0xF, both buttons' held counts cleared, mode 1 - and steps the
/// task on one state.
void func_dryfield_night_motel_lobby_80181298(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;
    s32               i;

    for (i = 0; i < 2; i++, prompt++) {
        prompt->field_0               = 0;
        prompt->field_4               = 0;
        prompt->targetId              = 0x100;
        prompt->field_E               = 0xF;
        prompt->buttons[0].heldFrames = 0;
        prompt->buttons[1].heldFrames = 0;
        prompt->mode                  = 1;
    }
    task->state = task->state + 1;
}
