#include "common.h"

#include "actors/actor_105500.h"
#include "gameplay/3A34.h"

/// Applies what the actor's gameplay record reports. Flag 2 (cleared here)
/// sends the actor to action 7 and sets `field_3D2`; flags 4/8 bring the damage
/// `Gp_TickObjFlag4` returns, which is reported through `func_800DA6E8`,
/// taken off the hit points in `field_40`, and answered with action 9 and
/// `field_30 = 2` when they run out or action 6 otherwise. Neither reaction
/// happens while `field_3C8` is 1. Flags 4/8 are cleared once
/// `Gp_ObjFlag4Expired` reports them spent.
void Actor05500_Fn03674(Actor105500* arg0, Actor105500Obj2C* arg1, s32 arg2)
{
    Actor105500Work* work;
    s32              damage;
    s32              remaining;
    u8               flags;
    Actor105500Ctx*  ctx;

    ctx   = arg0->field_20;
    flags = ctx->field_4C;
    work  = arg0->field_1C;
    if ((flags & 2) && (work->field_3C8 != 1)) {
        ctx->field_4C   = (u8)(flags & 0xFD);
        work->field_39A = 7;
        work->field_39C = 0;
        work->field_3D2 = 1;
    }
    if (ctx->field_4C & 0xC) {
        damage = Gp_TickObjFlag4((GpObj5C*)ctx);
        if ((s16)damage != 0) {
            func_800DA6E8(&ctx->field_10, (s16)damage, 0);
            remaining     = (u16)ctx->field_40 - damage;
            ctx->field_40 = remaining;
            if (work->field_3C8 != 1) {
                if ((s16)remaining <= 0) {
                    work->field_39A = 9;
                    work->field_39C = 0;
                    arg0->field_30  = 2;
                } else {
                    work->field_39A = 6;
                    work->field_39C = 0;
                }
            }
        }
        if (Gp_ObjFlag4Expired((GpObj5C*)ctx) != 0) {
            ctx->field_4C = (u8)(ctx->field_4C & 0xF3);
        }
    }
}
