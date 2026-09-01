#include "common.h"

#include "actors/actor_210700.h"

void func_actor_210700_8014A208(Task* arg0)
{
    TmdObject*       ext;
    Actor210700Work* work;

    work          = (Actor210700Work*)arg0->idMap;
    ext           = arg0->extra;
    ext->field_1C = &work->light;
    ext->field_20 = &work->color;
}

INCLUDE_ASM("actors/nonmatchings/actor_210700/actor_210700_2", func_actor_210700_8014A224);

INCLUDE_ASM("actors/nonmatchings/actor_210700/actor_210700_2", func_actor_210700_8014A344);

INCLUDE_ASM("actors/nonmatchings/actor_210700/actor_210700_2", func_actor_210700_8014A3D4);

INCLUDE_ASM("actors/nonmatchings/actor_210700/actor_210700_2", func_actor_210700_8014A4B0);
