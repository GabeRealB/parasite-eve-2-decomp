/* Part of the Maggot and Caterpillar library; see maggot_caterpillar.h. */

/// Status handling, run when the enemy's `reactionFlags` are non-zero.
/// Buildup is consumed, unless `field_3C8` is 1, by switching to state 7 with
/// `field_3D2` set. While damage over time is set, `Gp_TickObjFlag4` yields a
/// per-frame damage that is passed to `func_800DA6E8` and taken from `hp`;
/// outside `field_3C8` 1 the actor then enters state 9 when they
/// run out (setting `field_30` to 2) or state 6 otherwise. The bits are
/// cleared once `Gp_ObjFlag4Expired` returns non-zero.
void maggotCaterpillarApplyStatus(Task* arg0)
{
    Actor105500Work* work;
    s32              damage;
    s32              remaining;
    u8               flags;
    Enemy*           ctx;

    ctx   = arg0->spawnArg2.pointer;
    flags = ctx->reactionFlags;
    work  = arg0->work;
    if ((flags & ENEMY_REACTION_BUILDUP) && (work->field_3C8 != 1)) {
        ctx->reactionFlags = (u8)(flags & ENEMY_REACTION_BUILDUP_CLEAR);
        work->field_39A    = 7;
        work->field_39C    = 0;
        work->field_3D2    = 1;
    }
    if (ctx->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
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
            ctx->reactionFlags = (u8)(ctx->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR);
        }
    }
}
