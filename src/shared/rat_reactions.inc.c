/* Part of the Rat library; see rat.h. */

/// Applies stagger, build-up and damage-over-time reactions to the rat.
///
/// Requires live work and an `Enemy` in `Task::spawnArg2.pointer`. Stagger is consumed before
/// build-up is considered; damage can override either with hurt or death.
/// Health subtraction retains its low 16 bits and tests them as signed.
static void _ratReactions(Task* actor)
{
    Enemy*   enemy;
    RatWork* work;
    s32      damage;
    u16      remainingHp;
    u8       reactionFlags;

    enemy         = actor->spawnArg2.pointer;
    reactionFlags = enemy->reactionFlags;
    work          = actor->work;
    if (reactionFlags & ENEMY_REACTION_STAGGER) {
        enemy->reactionFlags = reactionFlags & ENEMY_REACTION_STAGGER_CLEAR;
        work->mode           = RAT_MODE_STAGGER;
        work->step           = RAT_STAGGER_STEP_BEGIN;
    }
    if ((enemy->reactionFlags & ENEMY_REACTION_BUILDUP) && (work->mode != RAT_MODE_STAGGER && work->mode != RAT_MODE_BUILDUP)) {
        work->mode        = RAT_MODE_BUILDUP;
        work->step        = RAT_BUILDUP_STEP_BEGIN;
        work->buildupHeld = 1;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        damage = damageTickEnemyDamageOverTime(enemy);
        if (damage != 0) {
            worldTargetAddReadoutAmount(&enemy->node, damage, 0);
            remainingHp = enemy->hp - damage;
            enemy->hp   = remainingHp;
            if ((s16)remainingHp <= 0) {
                work->mode   = RAT_MODE_DEAD;
                work->step   = 0;
                actor->state = RAT_TASK_DEATH;
            } else {
                work->mode = RAT_MODE_HURT;
                work->step = RAT_HURT_STEP_BEGIN;
            }
        }
        if (damageIsEnemyDamageOverTimeExpired(enemy) != 0) {
            enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
        }
    }
}
