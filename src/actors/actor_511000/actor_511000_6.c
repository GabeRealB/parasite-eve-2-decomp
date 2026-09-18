#include "common.h"

#include "actors/actor_511000.h"
#include "gameplay/1BC.h"
#include "main/task.h"

/// Copies the animation id from `preset` into the work block parked in
/// `task->work`, reseeds slots 1..0x12 through `Gp_AnimResetSlot`, and
/// clears `field_480`'s halfword.
s32 func_actor_511000_80133DEC(Task* task, s32 arg1, Actor511000AnimPreset* preset)
{
    Actor511000Work2* work;
    s32               i;

    work            = (Actor511000Work2*)task->work;
    work->field_47C = preset->field_4;
    i               = 1;
    do {
        Gp_AnimResetSlot((GpAnimCtx*)work, i, work->field_47C);
        i++;
    } while (i < 0x13);
    work->field_480.half = 0;
    return 0;
}
