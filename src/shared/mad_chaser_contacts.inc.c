#include "gameplay/room_effects.h"

/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Per-frame contact handling for the overlay's enemy. Walks the eight
/// contact records: kind 1 (skipped when `arg1` is set) and kind 3 push the
/// model out, kind 2 applies a hit - damage, status effects and the pending
/// state request in `hitReaction` - unless `hitCooldown` is still cooling down.
/// Then ticks the status flags, applies `worldCollisionResolvePushback`'s collision step
/// (snapping back to `prevRootPos` when it reports a conflict) and moves the
/// root by the combined step and push-out.
void madChaserApplyContacts(Task* arg0, s16 arg1)
{
    WorldCollisionDelta delta;
    SVECTOR             push;
    s16                 maxX;
    s16                 maxZ;
    s16                 stepX;
    s16                 stepZ;
    u8                  blocked;
    MadChaserWork*      work;
    Enemy*              enemy;
    GfxCoord*           coord;
    s16                 amount;
    s32                 dmg;
    s32                 tmp;
    s16                 tick;
    s32                 i;

    stepZ   = 0;
    maxX    = 0;
    maxZ    = 0;
    stepX   = 0;
    blocked = 0;
    work    = (MadChaserWork*)arg0->work;
    coord   = arg0->extra.tmd->coords;
    enemy   = arg0->spawnArg2.pointer;
    SCRATCH_STACK_RESERVE_BYTES(8);
    work->hitTaken = 0;
    for (i = 0; i < 8; i++) {
        switch (work->contacts[i].key.value & 0xFFFF0000) {
            case 0x10000:
                if (arg1 != 0) {
                    break;
                }
            case 0x30000:
                madChaserCalcPush(arg0, coord, &work->contacts[i], &push);
                if (ABS(maxX) < ABS(push.vx)) {
                    maxX = push.vx;
                }
                if (ABS(maxZ) < ABS(push.vz)) {
                    maxZ = push.vz;
                }
                break;
            case 0x20000:
                if (work->hitCooldown == 0) {
                    work->hitTaken    = 1;
                    dmg               = damageComputePlayerAttack(work->contacts[i].key.value, work->playerDist, 0, 0);
                    amount            = dmg;
                    work->hitCooldown = damageGetPlayerAttackHitCooldown(work->contacts[i].key.value);
                    if (damageRollCriticalHit(enemy, work->contacts[i].key.value, 0) != 0) {
                        amount = ((u32)dmg << 16) >> 14;
                        effectSpawn(EFFECT_CRITICAL_HIT, &arg0->extra.tmd->coords[3], 0, NULL);
                    }
                    damageAccumulateLifeDrainHp(enemy, work->contacts[i].key.value, amount, 0);
                    worldTargetAddReadoutAmount(&enemy->node, amount, 0);
                    enemy->hp -= amount;
                    if (enemy->hp < 0) {
                        enemy->hp = 0;
                    }
                    effectSpawnHit(damageGetPlayerAttackEffectId(work->contacts[i].key.value),
                                   &arg0->extra.tmd->coords[1], NULL, &work->effectArg);
                    if (amount >= 0x28) {
                        work->hitReaction = MAD_CHASER_HIT_REACTION_HEAVY;
                    } else {
                        work->hitReaction = MAD_CHASER_HIT_REACTION_LIGHT;
                    }
                    switch (damageGetPlayerAttackReaction(work->contacts[i].key.value) & 0xFFFF) {
                        case DAMAGE_PLAYER_REACTION_NONE:
                            break;
                        case DAMAGE_PLAYER_REACTION_STAGGER:
                            damageStartEnemyStagger(enemy);
                            break;
                        case DAMAGE_PLAYER_REACTION_BUILDUP:
                            damageStartEnemyBuildup(enemy, work->contacts[i].key.value, 0);
                            break;
                        case DAMAGE_PLAYER_REACTION_POISON:
                            damageTryStartEnemyDamageOverTime(enemy, work->contacts[i].key.value, 0);
                            break;
                        case 4:
                            work->hitReaction = MAD_CHASER_HIT_REACTION_BLAST;
                            break;
                        case 5:
                            work->hitReaction = MAD_CHASER_HIT_REACTION_HEAVY;
                            break;
                        case DAMAGE_PLAYER_REACTION_EXPLOSION:
                            work->hitReaction = MAD_CHASER_HIT_REACTION_BLAST;
                            break;
                        case DAMAGE_PLAYER_REACTION_INCENDIARY:
                            work->hitReaction = MAD_CHASER_HIT_REACTION_HEAVY;
                            break;
                        case 8:
                            work->hitReaction = MAD_CHASER_HIT_REACTION_STATUS;
                            break;
                        case 9:
                            work->hitReaction = MAD_CHASER_HIT_REACTION_STATUS;
                            break;
                    }
                } else if ((damageGetPlayerAttackEffectId(work->contacts[i].key.value)) == 0xD) {
                    effectSpawnHit(EFFECT_HIT_KIND_LIFE_DRAIN_MOTES, &arg0->extra.tmd->coords[1], NULL, &work->effectArg);
                }
                break;
        }
    }

    if (enemy->reactionFlags & ENEMY_REACTION_STAGGER) {
        enemy->reactionFlags &= 0xFE;
        work->hitReaction     = MAD_CHASER_HIT_REACTION_KNOCKDOWN;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
        enemy->reactionFlags &= 0xFD;
        work->hitReaction     = MAD_CHASER_HIT_REACTION_STATUS;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        work->damageOverTimeSeen = 1;
        tmp                      = damageTickEnemyDamageOverTime(enemy);
        tick                     = tmp;
        if (tick != 0) {
            enemy->hp -= tmp;
            worldTargetAddReadoutAmount(&enemy->node, tick, 0);
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

    switch (worldCollisionResolvePushback(work->contacts, &delta, 8, NULL)) {
        case WORLD_COLLISION_PUSHBACK_NO_GRID_HIT:
            break;
        case WORLD_COLLISION_PUSHBACK_GRID_HIT:
            stepZ = delta.fixed.vz.halves.integer;
            stepX = delta.fixed.vx.word >> 16;
            if (delta.fixed.vx.word & 0xFFFF) {
                if (delta.fixed.vx.word > 0) {
                    stepX++;
                } else {
                    stepX--;
                }
            }
            if (delta.fixed.vz.word & 0xFFFF) {
                if (delta.fixed.vz.word > 0) {
                    stepZ++;
                } else {
                    stepZ--;
                }
            }
            break;
        case WORLD_COLLISION_PUSHBACK_OPPOSED:
            coord->coord.t[0]   = work->prevRootPos.vx;
            coord->coord.t[2]   = work->prevRootPos.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            blocked             = 1;
            break;
    }

    worldCollisionClearContacts(work->contacts);
    if (work->field_43E != 0) {
        work->field_43E--;
    }
    if (work->hitCooldown > 0) {
        work->hitCooldown--;
    }
    if (blocked == 0) {
        work->anchorPos.vx += actorPickStep(stepX, maxX >> 3);
        work->anchorPos.vz += actorPickStep(stepZ, maxZ >> 3);
        coord->coord.t[0]  += actorPickStep(stepX, maxX >> 3);
        coord->coord.t[2]  += actorPickStep(stepZ, maxZ >> 3);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    SCRATCH_STACK_RELEASE_BYTES(8);
}
