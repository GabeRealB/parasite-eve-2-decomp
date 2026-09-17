#include "common.h"

#include "actors/actor_201200.h"

s32 func_actor_201200_8014D8DC(Actor201200* arg0, s32 arg1, Actor201200Msg* arg2)
{
    Actor201200Work* work;
    Actor201200Ctx*  ctx;

    work            = arg0->field_1C;
    ctx             = arg0->field_20;
    work->field_194 = arg2->bytes.b0;
    work->field_195 = arg2->bytes.b1;
    work->field_196 = arg2->bytes.b2;
    if (arg2->words.type == 0xB02) {
        switch (arg2->words.cmd) {
            case 0:
                work->field_0 = 0;
                break;
            case 1:
            case 2:
                break;
            case 3:
                if (ctx->field_40 > 0) {
                    work->field_0                = 4;
                    arg0->field_2C->field_8->flg = 0;
                }
                break;
            case 4:
                if (ctx->field_40 > 0) {
                    work->field_0 = 9;
                }
                break;
        }
    }
    return 0;
}
