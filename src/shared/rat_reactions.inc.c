/* Part of the Rat library; see rat.h. */

/// Folds the enemy's reaction flags into the behaviour mode: stagger switches
/// to mode 2, build-up to mode 3 (unless already in 2 or 3). Damage-over-time
/// ticks Gp_TickObjFlag4 damage into the hit points, entering death (task state
/// 2) or the hurt mode 4, and clears those flags once expired.
void ratReactions(Task* arg0)
{
    Enemy*   ctx;
    RatWork* work;
    s32      damage;
    u16      remaining;
    u8       flags;

    ctx   = arg0->spawnArg2.pointer;
    flags = ctx->reactionFlags;
    work  = arg0->work;
    if (flags & ENEMY_REACTION_STAGGER) {
        ctx->reactionFlags = flags & ENEMY_REACTION_STAGGER_CLEAR;
        work->mode         = RAT_MODE_STAGGER;
        work->step         = 0;
    }
    if ((ctx->reactionFlags & ENEMY_REACTION_BUILDUP) && (work->mode != RAT_MODE_STAGGER && work->mode != RAT_MODE_BUILDUP)) {
        work->mode        = RAT_MODE_BUILDUP;
        work->step        = 0;
        work->buildupHeld = 1;
    }
    if (ctx->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        damage = Gp_TickObjFlag4(ctx);
        if (damage != 0) {
            func_800DA6E8(&ctx->node, damage, 0);
            remaining = ctx->hp - damage;
            ctx->hp   = remaining;
            if ((s16)remaining <= 0) {
                work->mode  = RAT_MODE_DEAD;
                work->step  = 0;
                arg0->state = 2;
            } else {
                work->mode = RAT_MODE_HURT;
                work->step = 0;
            }
        }
        if (Gp_ObjFlag4Expired(ctx) != 0) {
            ctx->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
        }
    }
}
