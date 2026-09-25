#include "common.h"

#include "actors/actor_450800.h"

/// Message handler 0x7D3 of `D_actor_450800_801539AC`, the enemy's "start
/// animation" opcode: `withArg` selects between the two start paths
/// `func_actor_450800_801330AC` dispatches on, and only the first carries
/// `animArg`. Returns -1, without touching the work block, when the clip id is
/// out of range.
///
/// The `SOFT_BARRIER()` is a codegen pin, not a semantic one. Without it GCC's
/// delay-slot pass fills the `beqz` from the fall-through arm (`state = 1`);
/// the ROM has the *else* arm's `state = 2` there, which the pass only reaches
/// once an `asm` at the head of the fall-through stops it searching that
/// thread. See DECOMPILATION_LEARNINGS.md, "An empty asm at the head of the
/// then-arm moves the delay slot to the else arm".
s32 func_actor_450800_80133528(Task* task, s32 arg1, Actor450800AnimArgs* args)
{
    Actor450800SpawnWork* work;

    work = (Actor450800SpawnWork*)task->work;
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
    func_actor_450800_801330AC(task);
    return 0;
}
