#include "common.h"

#include "actors/actor_135400.h"

#include "main/tmd.h"

/// Points the main task's model at the work block's own light and colour
/// matrices, so it draws with the actor's lighting rather than the defaults.
/// The spawn handler runs it once the two part tasks are started.
void func_actor_135400_80132634(Task* task)
{
    TmdObject*           ext;
    Actor135400MainWork* work;

    ext           = task->extra;
    work          = (Actor135400MainWork*)task->work;
    ext->lightMtx = &work->lightMtx;
    ext->colorMtx = &work->colorMtx;
}
