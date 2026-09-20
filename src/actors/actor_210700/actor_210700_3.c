#include "common.h"

#include "actors/actor_210700.h"

void func_actor_210700_8014A208(Task* arg0)
{
    TmdObject*       ext;
    Actor210700Work* work;

    work          = (Actor210700Work*)arg0->work;
    ext           = arg0->extra;
    ext->lightMtx = &work->light;
    ext->colorMtx = &work->color;
}
