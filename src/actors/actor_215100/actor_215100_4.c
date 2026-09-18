#include "common.h"

#include "actors/actor_215100.h"
#include "actors/actors_shared_8014c874.h"

/// Script opcode: start animation `args->animId` on this actor.
///
/// `withArg` selects between the two start paths `ActorsShared8014c874`
/// dispatches on, and only the first carries `animArg`. Returns -1, without
/// touching the work block, when the clip id is out of range.
///
/// The `SOFT_BARRIER()` is a codegen pin, not a semantic one. Without it GCC's
/// delay-slot pass fills the `beqz` from the fall-through arm (`state = 1`);
/// the ROM has the *else* arm's `state = 2` there, which the pass only reaches
/// once an `asm` at the head of the fall-through stops it searching that
/// thread. See DECOMPILATION_LEARNINGS.md, "An empty asm at the head of the
/// then-arm moves the branch delay slot to the else arm".
s32 func_actor_215100_8014CCE0(Task* task, s32 arg1, Actor215100AnimArgs* args)
{
    Actor215100Work* work;

    work = (Actor215100Work*)task->work;
    if (args->animId >= 0x19) {
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
    ActorsShared8014c874(task);
    return 0;
}
