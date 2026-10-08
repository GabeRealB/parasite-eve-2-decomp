#include "gameplay/room_effects.h"

/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Applies Mad Chaser body contacts, hit reactions and horizontal correction.
///
/// Requires live enemy/model/work and eight writable body contacts. A nonzero
/// `ignorePlayerBody` suppresses player-body overlap push only. Attack damage
/// narrows to s16, including critical multiplication; hit cooldown gates later
/// contacts in the same pass and decreases at the end of this update.
/// Status damage still ticks without an attack contact. Opposed grid normals
/// restore saved root X/Z and suppress all anchor/root correction.
/// Otherwise each axis selects the grid step or one eighth of its strongest
/// overlap push. Fractional grid steps add their sign to the floored integer
/// half, including an extra negative unit. Y is retained. Clears the contacts
/// and releases its eight-byte scratch reservation before returning.
static void _madChaserApplyContacts(Task* task, s16 ignorePlayerBody)
{
    // Unnamed attributes are identified here only by this enemy's response.
    enum {
        MAD_CHASER_HEAVY_HIT_MIN_DAMAGE      = 40,
        MAD_CHASER_BODY_PUSH_DIVISOR_SHIFT   = 3,
        MAD_CHASER_ATTACK_ATTRIBUTE_BLAST_4  = 4,
        MAD_CHASER_ATTACK_ATTRIBUTE_HEAVY_5  = 5,
        MAD_CHASER_ATTACK_ATTRIBUTE_STATUS_8 = 8,
        MAD_CHASER_ATTACK_ATTRIBUTE_STATUS_9 = 9,
        MAD_CHASER_PUSH_FRACTION_BITS        = 16,
        MAD_CHASER_PUSH_FRACTION_MASK        = 0xFFFF
    };
    /// Adds the signed 16.16 correction's sign to its floored halfword step.
    ///
    /// Both arguments are side-effect-free scalar lvalues. correctionWord is
    /// read up to twice and gridStep once on the selected path; increments
    /// narrow to s16. Negative fractions subtract one below their integer half.
#define MAD_CHASER_ADJUST_FRACTIONAL_GRID_STEP(gridStep, correctionWord) \
    {                                                                    \
        if ((correctionWord) & MAD_CHASER_PUSH_FRACTION_MASK) {          \
            if ((correctionWord) > 0) {                                  \
                (gridStep)++;                                            \
            } else {                                                     \
                (gridStep)--;                                            \
            }                                                            \
        }                                                                \
    }
    WorldCollisionDelta gridCorrection;
    SVECTOR             contactPush;
    s16                 strongestPushX;
    s16                 strongestPushZ;
    s16                 gridStepX;
    s16                 gridStepZ;
    u8                  opposedGridNormals;
    MadChaserWork*      work;
    Enemy*              enemy;
    GfxCoord*           rootCoord;
    s16                 hitAmount;
    s32                 attackDamage;
    s32                 statusDamage;
    s16                 statusReadoutAmount;
    s32                 contactIndex;

    gridStepZ          = 0;
    strongestPushX     = 0;
    strongestPushZ     = 0;
    gridStepX          = 0;
    opposedGridNormals = 0;
    work               = task->work;
    rootCoord          = task->extra.tmd->coords;
    enemy              = task->spawnArg2.pointer;
    SCRATCH_STACK_RESERVE_BYTES(8);
    work->hitTaken = 0;
    // Accumulate body overlaps and apply attacks admitted by the hit cooldown.
    for (contactIndex = 0; contactIndex < ARRAY_SIZE(work->contacts); contactIndex++) {
        switch (work->contacts[contactIndex].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) {
            case WORLD_COLLISION_CONTACT_PLAYER_BODY:
                if (ignorePlayerBody != 0) {
                    break;
                }
            case WORLD_COLLISION_CONTACT_ENEMY_BODY:
                _madChaserCalcContactPushback(task, rootCoord, &work->contacts[contactIndex], &contactPush);
                if (ABS(strongestPushX) < ABS(contactPush.vx)) {
                    strongestPushX = contactPush.vx;
                }
                if (ABS(strongestPushZ) < ABS(contactPush.vz)) {
                    strongestPushZ = contactPush.vz;
                }
                break;
            case WORLD_COLLISION_CONTACT_ATTACK:
                if (work->hitCooldown == 0) {
                    work->hitTaken    = 1;
                    attackDamage      = damageComputePlayerAttack(work->contacts[contactIndex].key.value, work->playerDist, 0, 0);
                    hitAmount         = attackDamage;
                    work->hitCooldown = damageGetPlayerAttackHitCooldown(work->contacts[contactIndex].key.value);
                    if (damageRollCriticalHit(enemy, work->contacts[contactIndex].key.value, 0) != 0) {
                        hitAmount = ((u32)attackDamage << 16) >> 14;
                        effectSpawn(EFFECT_CRITICAL_HIT, &task->extra.tmd->coords[3], 0, NULL);
                    }
                    damageAccumulateLifeDrainHp(enemy, work->contacts[contactIndex].key.value, hitAmount, 0);
                    worldTargetAddReadoutAmount(&enemy->node, hitAmount, 0);
                    enemy->hp -= hitAmount;
                    if (enemy->hp < 0) {
                        enemy->hp = 0;
                    }
                    effectSpawnHit(damageGetPlayerAttackEffectId(work->contacts[contactIndex].key.value),
                                   &task->extra.tmd->coords[1], NULL, &work->effectArg);
                    if (hitAmount >= MAD_CHASER_HEAVY_HIT_MIN_DAMAGE) {
                        work->hitReaction = MAD_CHASER_HIT_REACTION_HEAVY;
                    } else {
                        work->hitReaction = MAD_CHASER_HIT_REACTION_LIGHT;
                    }
                    switch (damageGetPlayerAttackReaction(work->contacts[contactIndex].key.value) & 0xFFFF) {
                        case DAMAGE_PLAYER_REACTION_NONE:
                            break;
                        case DAMAGE_PLAYER_REACTION_STAGGER:
                            damageStartEnemyStagger(enemy);
                            break;
                        case DAMAGE_PLAYER_REACTION_BUILDUP:
                            damageStartEnemyBuildup(enemy, work->contacts[contactIndex].key.value, 0);
                            break;
                        case DAMAGE_PLAYER_REACTION_POISON:
                            damageTryStartEnemyDamageOverTime(enemy, work->contacts[contactIndex].key.value, 0);
                            break;
                        case MAD_CHASER_ATTACK_ATTRIBUTE_BLAST_4:
                            work->hitReaction = MAD_CHASER_HIT_REACTION_BLAST;
                            break;
                        case MAD_CHASER_ATTACK_ATTRIBUTE_HEAVY_5:
                            work->hitReaction = MAD_CHASER_HIT_REACTION_HEAVY;
                            break;
                        case DAMAGE_PLAYER_REACTION_EXPLOSION:
                            work->hitReaction = MAD_CHASER_HIT_REACTION_BLAST;
                            break;
                        case DAMAGE_PLAYER_REACTION_INCENDIARY:
                            work->hitReaction = MAD_CHASER_HIT_REACTION_HEAVY;
                            break;
                        case MAD_CHASER_ATTACK_ATTRIBUTE_STATUS_8:
                            work->hitReaction = MAD_CHASER_HIT_REACTION_STATUS;
                            break;
                        case MAD_CHASER_ATTACK_ATTRIBUTE_STATUS_9:
                            work->hitReaction = MAD_CHASER_HIT_REACTION_STATUS;
                            break;
                    }
                } else if ((damageGetPlayerAttackEffectId(work->contacts[contactIndex].key.value)) == EFFECT_HIT_KIND_LIFE_DRAIN_MOTES) {
                    effectSpawnHit(EFFECT_HIT_KIND_LIFE_DRAIN_MOTES, &task->extra.tmd->coords[1], NULL, &work->effectArg);
                }
                break;
        }
    }

    // Consume status reactions independently of this frame's weapon contacts.
    if (enemy->reactionFlags & ENEMY_REACTION_STAGGER) {
        enemy->reactionFlags &= ENEMY_REACTION_STAGGER_CLEAR;
        work->hitReaction     = MAD_CHASER_HIT_REACTION_KNOCKDOWN;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
        enemy->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
        work->hitReaction     = MAD_CHASER_HIT_REACTION_STATUS;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        work->damageOverTimeSeen = 1;
        statusDamage             = damageTickEnemyDamageOverTime(enemy);
        statusReadoutAmount      = statusDamage;
        if (statusReadoutAmount != 0) {
            enemy->hp -= statusDamage;
            worldTargetAddReadoutAmount(&enemy->node, statusReadoutAmount, 0);
            if (enemy->hp < 0) {
                enemy->hp = 0;
            }
            work->hitTaken    = 1;
            work->hitReaction = MAD_CHASER_HIT_REACTION_HEAVY;
        }
        if (damageIsEnemyDamageOverTimeExpired(enemy) != 0) {
            enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
        }
    }

    // Resolve grid normals before applying the strongest overlap on each axis.
    switch (worldCollisionResolvePushback(work->contacts, &gridCorrection, ARRAY_SIZE(work->contacts), NULL)) {
        case WORLD_COLLISION_PUSHBACK_NO_GRID_HIT:
            break;
        case WORLD_COLLISION_PUSHBACK_GRID_HIT:
            gridStepZ = gridCorrection.fixed.vz.halves.integer;
            gridStepX = gridCorrection.fixed.vx.word >> MAD_CHASER_PUSH_FRACTION_BITS;
            MAD_CHASER_ADJUST_FRACTIONAL_GRID_STEP(gridStepX, gridCorrection.fixed.vx.word);
            MAD_CHASER_ADJUST_FRACTIONAL_GRID_STEP(gridStepZ, gridCorrection.fixed.vz.word);
            break;
        case WORLD_COLLISION_PUSHBACK_OPPOSED:
            rootCoord->coord.t[0]   = work->prevRootPos.vx;
            rootCoord->coord.t[2]   = work->prevRootPos.vz;
            rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            opposedGridNormals      = 1;
            break;
    }

    worldCollisionClearContacts(work->contacts);
    if (work->field_43E != 0) {
        work->field_43E--;
    }
    if (work->hitCooldown > 0) {
        work->hitCooldown--;
    }
    if (opposedGridNormals == 0) {
        work->anchorPos.vx     += _actorContactSelectAxisPushback(gridStepX, strongestPushX >> MAD_CHASER_BODY_PUSH_DIVISOR_SHIFT);
        work->anchorPos.vz     += _actorContactSelectAxisPushback(gridStepZ, strongestPushZ >> MAD_CHASER_BODY_PUSH_DIVISOR_SHIFT);
        rootCoord->coord.t[0]  += _actorContactSelectAxisPushback(gridStepX, strongestPushX >> MAD_CHASER_BODY_PUSH_DIVISOR_SHIFT);
        rootCoord->coord.t[2]  += _actorContactSelectAxisPushback(gridStepZ, strongestPushZ >> MAD_CHASER_BODY_PUSH_DIVISOR_SHIFT);
        rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    SCRATCH_STACK_RELEASE_BYTES(8);
#undef MAD_CHASER_ADJUST_FRACTIONAL_GRID_STEP
}
