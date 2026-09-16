#include "common.h"

#include "actors/actors_shared_80133530.h"

#include "gameplay/3A34.h"

void ActorsShared80133530(ActorShared80133530* arg0)
{
    ActorShared80133530Work* work;
    ActorShared80133530Ctx*  ctx;
    s16                      timer;

    work = arg0->field_1C;
    ctx  = arg0->field_20;
    if ((work->field_336 == 0) && (ctx->field_40 < work->field_33C)) {
        timer           = work->field_33E - 1;
        work->field_33E = timer;
        if ((timer << 0x10) <= 0) {
            ctx->field_40 = ctx->field_40 + 1;
            func_800DA6E8(&ctx->node, -1, 0);
            work->field_33E = 5;
        }
    }
}
