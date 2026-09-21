#include "common.h"

#include "main/task.h"

/// Two-state dispatcher whose handler table is built on the stack.
/// `ActorsShared8013845cSub0` is overlay-local spawn/setup in the 101100-family
/// slots; actor_143000 and actor_548100 share a different state-0 body
/// (`actors_shared_8013845c_sub0`, same as `Room_Util04`).
/// `ActorsShared8013845cSub1` is shared among the 101100-family slots;
/// actor_143000 and actor_548100 keep their own state-1 bodies under the
/// same name.
void ActorsShared8013845cSub0(Task* task);
void ActorsShared8013845cSub1(Task* task);

void ActorsShared8013845c(Task* task)
{
    TaskFunc funcs[2] = {
        ActorsShared8013845cSub0,
        ActorsShared8013845cSub1,
    };

    funcs[task->state](task);
}
