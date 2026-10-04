/* Part of the Maggot and Caterpillar library; see maggot_caterpillar.h. */

/// Status handling, run when the enemy's `reactionFlags` are non-zero. Buildup
/// is consumed, unless `reactionMode` is
/// `MAGGOT_CATERPILLAR_REACTION_COMMITTED`, by switching to
/// `MAGGOT_CATERPILLAR_BEHAVIOUR_STUN` with `stunned` set. While damage over
/// time is set, `Gp_TickObjFlag4` yields a per-frame damage that is passed to
/// `func_800DA6E8` and taken from `hp`; outside that reaction mode the actor
/// then enters `MAGGOT_CATERPILLAR_BEHAVIOUR_DEAD` when they run out (setting
/// `field_30` to 2) or `MAGGOT_CATERPILLAR_BEHAVIOUR_HURT` otherwise. The bits
/// are cleared once `Gp_ObjFlag4Expired` returns non-zero.
void maggotCaterpillarApplyStatus(Task* arg0)
{
    MaggotCaterpillarWork* work;
    s32                    damage;
    s32                    remaining;
    u8                     flags;
    Enemy*                 ctx;

    ctx   = arg0->spawnArg2.pointer;
    flags = ctx->reactionFlags;
    work  = arg0->work;
    if ((flags & ENEMY_REACTION_BUILDUP) && (work->reactionMode != MAGGOT_CATERPILLAR_REACTION_COMMITTED)) {
        ctx->reactionFlags = (u8)(flags & ENEMY_REACTION_BUILDUP_CLEAR);
        work->behaviour    = MAGGOT_CATERPILLAR_BEHAVIOUR_STUN;
        work->step         = 0;
        work->stunned      = 1;
    }
    if (ctx->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        damage = Gp_TickObjFlag4(ctx);
        if ((s16)damage != 0) {
            func_800DA6E8(&ctx->node, (s16)damage, 0);
            remaining = (u16)ctx->hp - damage;
            ctx->hp   = remaining;
            if (work->reactionMode != MAGGOT_CATERPILLAR_REACTION_COMMITTED) {
                if ((s16)remaining <= 0) {
                    work->behaviour = MAGGOT_CATERPILLAR_BEHAVIOUR_DEAD;
                    work->step      = 0;
                    arg0->state     = 2;
                } else {
                    work->behaviour = MAGGOT_CATERPILLAR_BEHAVIOUR_HURT;
                    work->step      = 0;
                }
            }
        }
        if (Gp_ObjFlag4Expired(ctx) != 0) {
            ctx->reactionFlags = (u8)(ctx->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR);
        }
    }
}
