#include "common.h"

#include "actors/actor_207200.h"

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_5", func_actor_207200_8014D7E8);

/// Re-picks the model coordinate the context is driven from. While the actor
/// is idle (`work->field_4A6 == 0`) the angle to the player is measured and a
/// large deviation falls back to the second model part, otherwise the fourth
/// part is used.
void func_actor_207200_8014D8DC(Actor207200* arg0)
{
    Actor207200Work* work;
    Actor207200Ctx*  ctx;
    GsCOORDINATE2*   coord;
    s32              dist;
    s32              angle;

    work = arg0->field_1C;
    ctx  = arg0->field_20;
    if (work->field_4A6 != 0) {
        coord = (*(TmdObject**)&arg0->field_2C)->field_8 + 1;
    } else {
        angle = func_actor_207200_8014CE20((*(TmdObject**)&arg0->field_2C)->field_8, &dist);
        if (angle < 0) {
            angle = -angle;
        }
        if (angle < 0x400) {
            coord = (*(TmdObject**)&arg0->field_2C)->field_8 + 3;
        } else {
            coord = (*(TmdObject**)&arg0->field_2C)->field_8 + 1;
        }
    }
    ctx->field_18 = coord;
    Gp_LinkNode(&ctx->node);
}

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_5", func_actor_207200_8014D97C);
