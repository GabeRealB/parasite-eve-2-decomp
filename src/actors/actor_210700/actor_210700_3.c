#include "common.h"

#include "actors/actor_210700.h"

void func_actor_210700_8014A208(Task* arg0)
{
    TmdObject*       ext;
    Actor210700Work* work;

    work          = (Actor210700Work*)arg0->work;
    ext           = arg0->extra;
    ext->field_1C = &work->light;
    ext->field_20 = &work->color;
}

INCLUDE_ASM("actors/nonmatchings/actor_210700/actor_210700_3", func_actor_210700_8014A224);
