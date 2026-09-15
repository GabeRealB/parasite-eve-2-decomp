#include "common.h"

#include "actors/actor_356100.h"

INCLUDE_RODATA("actors/nonmatchings/actor_356100/actor_356100_2", ActorsShared80135df4Table);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100_2", func_actor_356100_8016A0B8);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100_2", func_actor_356100_8016A158);

void func_actor_356100_8016A1D8(Actor356100* arg0)
{
    TmdObject*       obj;
    Actor356100Work* work;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 1;
        obj->field_C                |= 0x80;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100_2", func_actor_356100_8016A21C);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100_2", func_actor_356100_8016A2AC);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100_2", func_actor_356100_8016A340);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100_2", func_actor_356100_8016A3D4);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100_2", func_actor_356100_8016A468);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100_2", func_actor_356100_8016A550);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100_2", func_actor_356100_8016A5DC);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100_2", func_actor_356100_8016A668);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100_2", func_actor_356100_8016A710);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100_2", func_actor_356100_8016A834);
