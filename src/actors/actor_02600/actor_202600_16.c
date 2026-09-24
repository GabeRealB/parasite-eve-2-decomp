#include "common.h"

#include "actors/actor_202600.h"
#include "gameplay/3A34.h"

/// Status handling, run when the context's `field_4C` flags are non-zero.
/// Flag 0x2 is consumed, unless `field_3C8` is 1, by switching to state 7 with
/// `field_3D2` set. While flag 0x4 or 0x8 is set, `Gp_TickObjFlag4` yields a
/// per-frame damage that is passed to `func_800DA6E8` and taken from the hit
/// points in `field_40`; outside `field_3C8` 1 the actor then enters state 9 when they
/// run out (setting `field_30` to 2) or state 6 otherwise. Both flags are
/// cleared once `Gp_ObjFlag4Expired` returns non-zero.
void Actor02600_Fn0366C(Actor202600* arg0)
{
    Actor202600Work* work;
    s32              damage;
    s32              remaining;
    u8               flags;
    Actor202600Ctx*  ctx;

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
