#include "common.h"

#include "actors/actor_207200.h"

void Gp_UnlinkNode(void* node);
void Gp_UnlinkObj(Actor207200Obj* node);
void Gp_EnemyTaskExit(Actor207200* task);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_2", func_actor_207200_8014B278);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_2", func_actor_207200_8014B628);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_2", func_actor_207200_8014B87C);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_2", func_actor_207200_8014BEF4);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_2", func_actor_207200_8014C870);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_2", func_actor_207200_8014CA84);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_2", func_actor_207200_8014CE20);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_2", func_actor_207200_8014CFEC);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_2", func_actor_207200_8014D128);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_2", func_actor_207200_8014D280);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_2", func_actor_207200_8014D2DC);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_2", func_actor_207200_8014D41C);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_2", func_actor_207200_8014D49C);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_2", func_actor_207200_8014D5C4);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_2", func_actor_207200_8014D65C);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_2", func_actor_207200_8014D70C);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_2", func_actor_207200_8014D77C);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_2", func_actor_207200_8014D7E8);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_2", func_actor_207200_8014D8DC);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_2", func_actor_207200_8014D97C);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_2", func_actor_207200_8014DAF8);

void func_actor_207200_8014DB4C(Actor207200* arg0)
{
    Actor207200Ctx*  ctx;
    Actor207200Work* work;

    ctx           = arg0->field_20;
    work          = arg0->field_1C;
    ctx->field_54 = 0;
    Gp_UnlinkNode(&ctx->node);
    Gp_UnlinkObj(&work->field_1DC);
    Gp_UnlinkObj(&work->field_214);
    Gp_UnlinkObj(&work->field_2C4);
    Gp_UnlinkObj(&work->field_374);
    Gp_UnlinkObj(&work->field_3AC);
    Gp_EnemyTaskExit(arg0);
}
