#include "common.h"

#include "actors/actor_143000.h"

#include "main/task.h"
#include "rooms/room_common.h"

/// State 0 of the action-prompt task: resets both prompt slots before the
/// first cursor frame -- clears the position accumulators and the two
/// buttons' held-frame counters, parks the target id (the cursor speed) at
/// 0x100 and `field_E` (the double-press window) at 0xF, marks the slot
/// highlighted -- and steps the task on one state.
void func_actor_143000_80133C90(Task* task)
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
