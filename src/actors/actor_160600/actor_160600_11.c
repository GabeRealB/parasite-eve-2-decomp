#include "common.h"

#include "main/task.h"

#include "actors/actor_160600.h"

/// Script opcode: sets the work block's `field_4EE`, which enables the
/// per-frame effect spawns, when the payload is exactly 1; any other payload
/// is ignored.
s32 func_actor_160600_8013268C(Task* task, s32 arg1, Actor160600FlagArgs* args)
{
    Actor160600Work* work;
    u16              value;

    value = args->value;
    work  = (Actor160600Work*)task->work;
    if (value == 1) {
        work->field_4EE = value;
    }
    return 0;
}
