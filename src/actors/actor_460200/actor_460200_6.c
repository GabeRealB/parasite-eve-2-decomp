#include "common.h"

#include "actors/actor_460200.h"

/// Script opcode: raise the work block's `field_4EE`, the head-turn flag, when
/// the payload is exactly 1. Any other payload is ignored and leaves the flag
/// as it was.
s32 func_actor_460200_80132C8C(Task* task, s32 arg1, Actor460200LookArgs* args)
{
    Actor460200Work* work;
    u16              look;

    look = args->look;
    work = (Actor460200Work*)task->work;
    if (look == 1) {
        work->field_4EE = look;
    }
    return 0;
}
