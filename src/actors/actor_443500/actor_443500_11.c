#include "common.h"

#include "actors/actor_443500.h"

#include "main/tmd.h"

/// Points the model's `TmdObject::lightMtx` / `colorMtx` at the work block's
/// own `light` / `color` matrices, so the actor draws with its own lighting.
void func_actor_443500_801327C4(Task* task)
{
    TmdObject*       ext;
    Actor443500Work* work;

    ext           = task->extra;
    work          = (Actor443500Work*)task->work;
    ext->lightMtx = &work->light;
    ext->colorMtx = &work->color;
}
