#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

#include "actors/actor_160600.h"

/// Script opcode: start animation `args->animId` on this actor.
///
/// `withArg` selects between the two start paths the step body
/// `func_actor_160600_80131FFC` dispatches on, and only the first carries
/// `animArg`. Returns -1, without
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
    func_actor_160600_80131FFC(task);
    return 0;
}

/// Script opcode: shows or hides this actor's model and the model of the task
/// parked in `pairTask`. With `flags` bit 0 both models get `TmdObject::flags`
/// 0, which shows them; without it they get 0x80, which hides them. Bit 1
/// additionally ORs in 0x4. With `Task::spawnArg1` clear the actor drives its
/// own model twice.
s32 func_actor_160600_80132598(Task* task, s32 arg1, s32 flags)
{
    Actor160600Work* work;
    TmdObject*       self;
    TmdObject*       other;

    self = (TmdObject*)task->extra;
    work = (Actor160600Work*)task->work;
    if (task->spawnArg1 != 0) {
        other = (TmdObject*)work->pairTask->extra;
    } else {
        other = self;
    }
    if (flags & 1) {
        self->flags  = 0;
        other->flags = 0;
    } else {
        self->flags  = 0x80;
        other->flags = 0x80;
    }
    if (flags & 2) {
        self->flags  |= 4;
        other->flags |= 4;
    }
    return 0;
}
