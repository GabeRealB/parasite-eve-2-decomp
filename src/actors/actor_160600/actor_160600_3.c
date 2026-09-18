#include "common.h"

#include "actors/actor_160600.h"
#include "actors/actors_shared_8014c874.h"

/// Script opcode: start animation `args->animId` on this actor.
///
/// `withArg` selects between the two start paths `ActorsShared8014c874`
/// dispatches on, and only the first carries `animArg`. Returns -1, without
/// touching the work block, when the clip id is out of range.
///
/// Both halves of the shape are load-bearing, not stylistic. The two bare
/// `return`s put the constants straight into `$v0`, where the ROM has them; an
/// m2c-style `var_v0` accumulator instead colours them into the free `$a1` and
/// costs a tail `move $v0,$a1` (84.59%). The `SOFT_BARRIER()` moves the second
/// `beqz`'s delay slot from the fall-through arm (`state = 1`) to the else arm
/// (`state = 2`), which is what the ROM has; without it the function is 92.37%.
/// See DECOMPILATION_LEARNINGS.md, "An empty `asm` at the head of the then-arm
/// moves the branch delay slot to the else arm".
s32 func_actor_160600_8013252C(Task* task, s32 arg1, Actor160600AnimArgs* args)
{
    Actor160600Work* work;

    work = (Actor160600Work*)task->work;
    if (args->animId >= 0x10) {
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

INCLUDE_ASM("actors/nonmatchings/actor_160600/actor_160600_3", func_actor_160600_80132598);
