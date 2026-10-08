/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Advances swelling, buildup hold and damage-over-time requests on the Enemy.
///
/// Requires the live task/work/Enemy. STAGGER grows Q12 scale by 200 per call and
/// starts death on the fifth; its bit remains set. BUILDUP is consumed, stops
/// movement and freezes animation until the behaviour dispatch releases it.
/// Damage-over-time ticks feed their HP amount to `_sucklercephTakeDamage` and
/// clear both status bits on expiry. Later tests reload flags after earlier calls.
static void _sucklercephReactionFlags(Task* task)
{
    SucklercephWork* work;
    Enemy*           enemy;
    s32              damageOverTime;
    u8               initialReactionFlags;

    enemy                = task->spawnArg2.pointer;
    initialReactionFlags = enemy->reactionFlags;
    work                 = task->work;
    if (initialReactionFlags != 0) {
        if (initialReactionFlags & ENEMY_REACTION_STAGGER) {
            work->swellFrames += 1;
            work->swellScale  += SUCKLERCEPH_SWELL_SCALE_STEP_Q12;
            if (work->swellFrames >= SUCKLERCEPH_SWELL_DURATION_FRAMES) {
                _sucklercephKill(task, 0);
                task->killCountdown = SUCKLERCEPH_DEATH_COUNTDOWN_FRAMES;
                work->deathPhase    = SUCKLERCEPH_DEATH_PHASE_COUNTDOWN;
                task->state         = SUCKLERCEPH_TASK_DEATH;
                enemy->hp           = 0;
            }
        }
        if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
            enemy->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
            work->state           = SUCKLERCEPH_STATE_STATUS_HOLD;
            work->deathFrames     = 0;
            work->forwardSpeed    = 0;
            work->animFrozen      = 1;
        }
        if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
            damageOverTime = damageTickEnemyDamageOverTime(enemy);
            if (damageOverTime != 0) {
                _sucklercephTakeDamage(task, damageOverTime);
            }
            if (damageIsEnemyDamageOverTimeExpired(enemy) != 0) {
                enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
            }
        }
    }
}
