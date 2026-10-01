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
        work->field_37A    = 2;
        work->field_37C    = 0;
    }
    if ((ctx->reactionFlags & ENEMY_REACTION_BUILDUP) && ((u32)((u16)work->field_37A - 2) >= 2U)) {
        work->field_37A = 3;
        work->field_37C = 0;
        work->field_398 = 1;
    }
    if (ctx->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        damage = Gp_TickObjFlag4(ctx);
        if (damage != 0) {
            func_800DA6E8(&ctx->node, damage, 0);
            remaining = ctx->hp - damage;
            ctx->hp   = remaining;
            if ((s16)remaining <= 0) {
                work->field_37A = 5;
                work->field_37C = 0;
                arg0->state     = 2;
            } else {
                work->field_37A = 4;
                work->field_37C = 0;
            }
        }
        if (Gp_ObjFlag4Expired(ctx) != 0) {
            ctx->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
        }
    }
}
