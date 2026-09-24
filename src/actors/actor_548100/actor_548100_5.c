#include "common.h"

#include "actors/actor_548100.h"

#include "gameplay/3688.h"
#include "main/task.h"
#include "rooms/room_common.h"

/// State 1 of the actor's callback: arms the first action-prompt slot with
/// target id 0x80, marks it highlighted (`mode` 1), clears its screen position
/// and steps the task on to state 2.
void func_actor_548100_80134D88(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;

    prompt->targetId    = 0x80;
    prompt->mode        = 1;
    prompt->screen.xy.x = 0;
    prompt->screen.xy.y = 0;
    task->state         = task->state + 1;
}

/// State 3 of the actor's callback, entered once a hotspot is picked: clears
/// the prompt's highlight and target, re-spawns the prompt at its current
/// screen position with the picked hotspot's `promptKind`, and moves the task
/// to the `step` switch in state 4.
void func_actor_548100_80134DBC(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;
    Actor548100Work*  work   = (Actor548100Work*)task->work;

    prompt->mode     = 0;
    prompt->targetId = 0;
    func_800D4E78(prompt->screen.xy.x, prompt->screen.xy.y, work->promptKind);
    task->state = 4;
}
