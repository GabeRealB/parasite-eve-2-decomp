#include "common.h"

#include "actors/actor_205200.h"

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200_3", func_actor_205200_8014C8D4);

void func_actor_205200_8014C924(Actor205200Ctx* arg0, Actor205200* arg1)
{
    Actor205200Work* work;

    work = arg1->field_1C;
    Gp_UnlinkNode(&arg0->node);
    Gp_UnlinkObj(&work->field_47C);
    Gp_UnlinkObj(&work->field_4E4);
    Gp_DestroyEnemy(arg0, arg1);
}

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200_3", func_actor_205200_8014C980);

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200_3", func_actor_205200_8014C9A0);
