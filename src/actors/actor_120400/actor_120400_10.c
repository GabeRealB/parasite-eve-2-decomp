#include "common.h"

#include "actors/actor_120400.h"

#include "main/tmd.h"

/// Points the parent's model at the light and colour matrices held in its own
/// work block.
void func_actor_120400_801327D4(Task* task)
{
    TmdObject*           ext;
    Actor120400MainWork* work;

    ext           = task->extra;
    work          = (Actor120400MainWork*)task->work;
    ext->lightMtx = &work->light;
    ext->colorMtx = &work->color;
}
