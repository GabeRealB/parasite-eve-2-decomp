#include "common.h"

#include "actors/actor_213100.h"

void func_actor_213100_8014A23C(Task* arg0)
{
    TmdObject*       ext;
    Actor213100Work* work;

    ext           = arg0->extra;
    work          = (Actor213100Work*)arg0->idMap;
    ext->field_1C = &work->light;
    ext->field_20 = &work->color;
}

INCLUDE_ASM("actors/nonmatchings/actor_213100/actor_213100_2", func_actor_213100_8014A258);

INCLUDE_ASM("actors/nonmatchings/actor_213100/actor_213100_2", func_actor_213100_8014A390);

INCLUDE_ASM("actors/nonmatchings/actor_213100/actor_213100_2", func_actor_213100_8014A40C);
