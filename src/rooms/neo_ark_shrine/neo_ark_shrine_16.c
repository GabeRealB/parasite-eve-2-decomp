#include "common.h"

#include "main/task.h"
#include "rooms/room_common.h"

/// Resets both action-prompt slots and steps the task on: zeroes each slot's
/// fixed-point cursor position and its buttons' hold counters, sets
/// `targetId` to 0x100, `field_E` to 0xF and `mode` to 1.
void func_neo_ark_shrine_8017F80C(Task* task)
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
