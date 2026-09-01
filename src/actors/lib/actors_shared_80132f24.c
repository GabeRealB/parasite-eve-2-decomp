#include "common.h"

#include "actors/actors_shared_80132f24.h"

#include "main/tmd.h"

void ActorsShared80132f24(Task* task)
{
    TmdObject*                ext;
    ActorsShared80132f24Work* work;

    ext           = task->extra;
    work          = (ActorsShared80132f24Work*)task->idMap;
    ext->field_1C = &work->light;
    ext->field_20 = &work->color;
}
