#include "common.h"

#include "actors/actor_103700.h"

/// Handler for message 0x7DE. Ignored unless the actor is in one of its
/// active modes (below 7) and the task is in its tick state. While the actor
/// holds the player (`field_262`) it moves the hold to its release phase
/// (`field_250` = 3); otherwise it drops to mode 5 with the base animation
/// requested. Always answers 0.
s32 Actor03700_Fn034F8(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    Actor103700Work* work;
    s32              state;

    work = (Actor103700Work*)task->work;
    if (work->field_24E >= 7) {
        return 0;
    }
    state = task->state;
    if (state != 1) {
        return 0;
    }
    if (work->field_262 != 0) {
        work->field_250 = 3;
    } else {
        work->field_24E = 5;
        work->field_248 = state;
        work->field_250 = 0;
    }
    return 0;
}
