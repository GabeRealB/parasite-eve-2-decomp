#include "common.h"

#include "actors/actor_143000.h"

#include "gameplay/3688.h"
#include "main/task.h"
#include "rooms/room_common.h"

/// State 1 of the actor's callback: arms the first action-prompt slot with
/// target id 0x80, marks it highlighted (`mode` 1), clears its screen position
/// and steps the task on to state 2.
void func_actor_143000_80133664(Task* task)
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
/// to state 4.
void func_actor_143000_80133698(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;
    Actor143000Work*  work   = (Actor143000Work*)task->work;

    prompt->mode     = 0;
    prompt->targetId = 0;
    func_800D4E78(prompt->screen.xy.x, prompt->screen.xy.y, work->promptKind);
    task->state = 4;
}
