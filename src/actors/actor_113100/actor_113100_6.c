#include "common.h"

#include "actors/actor_113100.h"

#include "main/task.h"
#include "main/tmd.h"

void func_actor_113100_80132F24(Task* task)
{
    TmdObject*       ext;
    Actor113100Work* work;

    ext           = task->extra;
    work          = (Actor113100Work*)task->work;
    ext->lightMtx = &work->light;
    ext->colorMtx = &work->color;
}
