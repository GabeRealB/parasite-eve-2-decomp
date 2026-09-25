#include "common.h"

#include "actors/actor_161500.h"
#include "main/task.h"

/// Script opcode: starts clip `args->animId` on this actor, rejecting ids of
/// 0xC and above. With `args->withArg` set it queues the reseed that carries
/// `args->animArg` (state 1), otherwise the plain one (state 2), then runs the
/// step body at once so the new clip is seeded this frame.
///
/// The `SOFT_BARRIER` pins the delay slot of the `beqz`: without it the slot
/// fills from the fall-through arm (`state = 1`) instead of the else arm's
/// `state = 2`.
s32 func_actor_161500_80132A28(Task* task, s32 arg1, Actor161500AnimArgs* args)
{
    Actor161500Work* work;

    work = (Actor161500Work*)task->work;
    if (args->animId >= 0xC) {
        return -1;
    }

    work->animId = args->animId;
    if (args->withArg != 0) {
        SOFT_BARRIER();
        work->state   = 1;
        work->animArg = args->animArg;
    } else {
        work->state = 2;
    }
    work->field_4BA = 0;
    func_actor_161500_8013252C(task);
    return 0;
}
