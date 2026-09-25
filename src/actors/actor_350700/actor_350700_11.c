#include "common.h"

#include "actors/actor_350700.h"
#include "main/task.h"
#include "main/tmd.h"

/// Republishes the parent work block's two matrices onto
/// `TmdObject::lightMtx` / `colorMtx`, so the parent draws with its own
/// lighting; the counterpart of `func_actor_350700_801624B4` for
/// `Actor350700MainWork`.
void func_actor_350700_801633DC(Task* task)
{
    TmdObject*           ext;
    Actor350700MainWork* work;

    ext           = task->extra;
    work          = (Actor350700MainWork*)task->work;
    ext->lightMtx = &work->light;
    ext->colorMtx = &work->color;
}
