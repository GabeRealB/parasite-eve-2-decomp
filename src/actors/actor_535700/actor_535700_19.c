#include "common.h"

#include "actors/actor_535700.h"
#include "main/task.h"

/// "Start animation" opcode of the second enemy: `withArg` selects between the
/// two start paths `func_actor_535700_80132D68` dispatches on, and only the
/// first carries `animArg`. Returns -1, without touching the work block, when
/// the clip id is 6 or more.
///
/// The `SOFT_BARRIER()` is a codegen pin, not a semantic one: without it GCC's
/// delay-slot pass fills the `beqz` from the fall-through arm (`state = 1`)
/// rather than the else arm's `state = 2`, which the ROM has there.
s32 func_actor_535700_801331E4(Task* task, s32 arg1, Actor535700AnimArgs* args)
{
    Actor535700SpawnWork* work;

    work = (Actor535700SpawnWork*)task->work;
    if (args->animId >= 6) {
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
    work->field_482 = 0;
    func_actor_535700_80132D68(task);
    return 0;
}
