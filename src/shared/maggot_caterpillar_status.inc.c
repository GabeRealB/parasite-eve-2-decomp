/* Part of the Maggot and Caterpillar library; see maggot_caterpillar.h. */

/// Applies buildup and damage-over-time reactions to the actor.
///
/// Committed actions retain their behavior while HP changes; other actions
/// enter stun, hurt or the dying task phase. Damage is narrowed to a signed
/// halfword for the readout and zero test, and HP is stored as a halfword.
/// Expired damage-over-time flags are cleared after the pulse.
static void _maggotCaterpillarApplyStatus(Task* actor)
{
    MaggotCaterpillarWork* work;
    s32                    damage;
    s32                    remainingHp;
    u8                     reactionFlags;
    Enemy*                 enemy;

    enemy         = actor->spawnArg2.pointer;
    reactionFlags = enemy->reactionFlags;
    work          = actor->work;
    if ((reactionFlags & ENEMY_REACTION_BUILDUP) && (work->reactionMode != MAGGOT_CATERPILLAR_REACTION_COMMITTED)) {
        enemy->reactionFlags = (u8)(reactionFlags & ENEMY_REACTION_BUILDUP_CLEAR);
        work->behaviour      = MAGGOT_CATERPILLAR_BEHAVIOUR_STUN;
        work->step           = 0;
        work->stunned        = 1;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        damage = damageTickEnemyDamageOverTime(enemy);
        if ((s16)damage != 0) {
            worldTargetAddReadoutAmount(&enemy->node, (s16)damage, 0);
            // Preserve halfword HP arithmetic, including the unsigned input view.
            remainingHp = (u16)enemy->hp - damage;
            enemy->hp   = remainingHp;
            if (work->reactionMode != MAGGOT_CATERPILLAR_REACTION_COMMITTED) {
                if ((s16)remainingHp <= 0) {
                    work->behaviour = MAGGOT_CATERPILLAR_BEHAVIOUR_DEAD;
                    work->step      = 0;
                    actor->state    = MAGGOT_CATERPILLAR_TASK_DYING;
                } else {
                    work->behaviour = MAGGOT_CATERPILLAR_BEHAVIOUR_HURT;
                    work->step      = 0;
                }
            }
        }
        if (damageIsEnemyDamageOverTimeExpired(enemy) != 0) {
            enemy->reactionFlags = (u8)(enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR);
        }
    }
}
