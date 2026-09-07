#include "common.h"

#include "main/task.h"

/// Two-state dispatcher whose handler table is built on the stack. Each
/// overlay aliases its own state-0 / state-1 callbacks as
/// ActorsShared8013845cSub0 / ActorsShared8013845cSub1.
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
