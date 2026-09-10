#include "common.h"

#include "actors/actor_207200.h"

void Gp_UnlinkNode(void* node);
void Gp_UnlinkObj(Actor207200Obj* node);
void Gp_EnemyTaskExit(Actor207200* task);

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
