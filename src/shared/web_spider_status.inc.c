/* Part of the web spider library; see web_spider.h. */

/// Status handling, run when the context's `field_4C` flags are non-zero.
/// Flag 0x2 is consumed, unless `field_3C8` is 1, by switching to state 7 with
/// `field_3D2` set. While flag 0x4 or 0x8 is set, `Gp_TickObjFlag4` yields a
/// per-frame damage that is passed to `func_800DA6E8` and taken from the hit
/// points in `field_40`; outside `field_3C8` 1 the actor then enters state 9 when they
/// run out (setting `field_30` to 2) or state 6 otherwise. Both flags are
/// cleared once `Gp_ObjFlag4Expired` returns non-zero.
void spiderApplyStatus(Task* arg0)
{
    Actor105500Work* work;
    s32              damage;
    s32              remaining;
    u8               flags;
    GpEnemy*         ctx;

    ctx   = arg0->spawnArg2.pointer;
    flags = ctx->reactionFlags;
    work  = arg0->work;
    if ((flags & 2) && (work->field_3C8 != 1)) {
        ctx->reactionFlags = (u8)(flags & 0xFD);
        work->field_39A    = 7;
        work->field_39C    = 0;
        work->field_3D2    = 1;
    }
    if (ctx->reactionFlags & 0xC) {
        damage = Gp_TickObjFlag4(ctx);
        if ((s16)damage != 0) {
            func_800DA6E8(&ctx->node, (s16)damage, 0);
            remaining = (u16)ctx->hp - damage;
            ctx->hp   = remaining;
            if (work->field_3C8 != 1) {
                if ((s16)remaining <= 0) {
                    work->field_39A = 9;
                    work->field_39C = 0;
                    arg0->state     = 2;
                } else {
                    work->field_39A = 6;
                    work->field_39C = 0;
                }
            }
        }
        if (Gp_ObjFlag4Expired(ctx) != 0) {
            ctx->reactionFlags = (u8)(ctx->reactionFlags & 0xF3);
        }
    }
}
