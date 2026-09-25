#include "common.h"

#include "actors/actor_135600.h"

#include "main/task.h"
#include "main/tmd.h"

void func_actor_135600_80132DDC(Task* task)
{
    TmdObject*       ext;
    Actor135600Work* work;

    ext           = task->extra;
    work          = (Actor135600Work*)task->work;
    ext->lightMtx = &work->light;
    ext->colorMtx = &work->color;
}
