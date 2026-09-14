#include "common.h"

#include "actors/actor_143000.h"

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000_3", func_actor_143000_801336E8);

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000_3", func_actor_143000_80133800);

void func_actor_143000_801338C8(Actor143000* arg0)
{
    arg0->field_1C->field_4 = 0;
    arg0->field_30          = 2;
}

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000_3", func_actor_143000_801338E0);

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000_3", func_actor_143000_801339CC);

void func_actor_143000_80133AC0(Actor143000* arg0)
{
    u16 count = (u16)arg0->field_2A - 1;

    arg0->field_2A = count;
    if ((s16)count <= 0) {
        arg0->field_30 = 5;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000_3", func_actor_143000_80133AE8);

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000_3", func_actor_143000_80133C2C);

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000_3", ActorsShared8013845cSub0);

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000_3", func_actor_143000_80133CF0);
