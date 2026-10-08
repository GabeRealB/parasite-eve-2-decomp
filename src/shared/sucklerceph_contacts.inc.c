#include "gameplay/room_effects.h"

/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Resolves grid/body separation, player proximity and attacks, then consumes contacts.
///
/// Requires live task/work/Enemy storage, composed root and grid-view matrices,
/// and one free `SucklercephContactsScratch` block (76 bytes), plus callee scratch.
/// Grid correction adds Q16.16 integer halves or restores the previous step's
/// position. Player contact or XZ range below 800 starts swelling while crawling.
/// All four contacts are examined: critical attacks force a burst, ordinary
/// attacks apply HP/status/effects/cooldown, and enemy bodies separate the root
/// by their overlap in the grid's parent frame. Only negative Y body corrections apply.
/// Attack-body contact disables its pair tests once awake. No pointer is retained.
static void _sucklercephContacts(Task* task)
{
    enum {
        SUCKLERCEPH_SWELL_PLAYER_RANGE           = 800,
        SUCKLERCEPH_REACTION_LOW_MASK            = 0xFFFF,
        SUCKLERCEPH_REACTION_BUILDUP_ALTERNATE   = 9,
        SUCKLERCEPH_CONTACT_NORMAL_FRACTION_BITS = 12,
        SUCKLERCEPH_CRITICAL_DEATH_HP            = -1
    };
    Enemy*                      enemy;
    WorldCollisionContact*      attackContact;
    s32                         attackReaction;
    s32                         verticalPushQ12;
    s32                         gridResponse;
    s32                         playerDx;
    s32                         playerDz;
    s32                         bodyDx;
    s32                         bodyDz;
    s16                         attackCooldownFrames;
    s32                         rangeOrOverlap;
    u32                         attackDamage;
    SucklercephWork*            work;
    GfxCoord*                   rootCoord;
    SucklercephContactsScratch* scratch;
    s32                         contactIndex;

    work         = task->work;
    scratch      = SCRATCH_STACK_RESERVE_BLOCK(SucklercephContactsScratch);
    rootCoord    = task->extra.tmd->coords;
    enemy        = task->spawnArg2.pointer;
    gridResponse = worldCollisionResolvePushback(work->contacts, &scratch->delta, ARRAY_SIZE(work->contacts), &scratch->gridKeyMask);
    // Correct the last root step before measuring proximity and hit range.
    switch (gridResponse) {
        case WORLD_COLLISION_PUSHBACK_NO_GRID_HIT:
            break;
        case WORLD_COLLISION_PUSHBACK_GRID_HIT:
            rootCoord->coord.t[0] += scratch->delta.fixed.vx.halves.integer;
            rootCoord->coord.t[1] += scratch->delta.fixed.vy.halves.integer;
            rootCoord->coord.t[2] += scratch->delta.fixed.vz.halves.integer;
            break;
        case WORLD_COLLISION_PUSHBACK_OPPOSED:
            rootCoord->coord.t[0] = work->prevRootPos.vx;
            rootCoord->coord.t[1] = work->prevRootPos.vy;
            rootCoord->coord.t[2] = work->prevRootPos.vz;
            break;
    }
    if (work->hitCooldown != 0) {
        work->hitCooldown--;
        if (work->hitCooldown <= 0) {
            work->hitCooldown = 0;
        }
    }
    playerDx                 = gPlayerStatus.coordMtx->t[0] - rootCoord->coord.t[0];
    scratch->delta.vector.vx = playerDx;
    scratch->delta.vector.vy = gPlayerStatus.coordMtx->t[1] - rootCoord->coord.t[1];
    playerDz                 = gPlayerStatus.coordMtx->t[2] - rootCoord->coord.t[2];
    scratch->delta.vector.vz = playerDz;
    rangeOrOverlap           = SquareRoot0((playerDx * playerDx) + (playerDz * playerDz));
    if (rangeOrOverlap < SUCKLERCEPH_SWELL_PLAYER_RANGE && work->awakeStage == SUCKLERCEPH_AWAKE_STAGE_CRAWL) {
        work->animFrozen = 1;
        work->awakeStage = SUCKLERCEPH_AWAKE_STAGE_SWELL;
    }
    for (contactIndex = 0; contactIndex < ARRAY_SIZE(work->contacts); contactIndex++) {
        switch (work->contacts[contactIndex].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) {
            case WORLD_COLLISION_CONTACT_PLAYER_BODY:
                if (work->awakeStage == SUCKLERCEPH_AWAKE_STAGE_CRAWL) {
                    work->animFrozen = 1;
                    work->awakeStage = SUCKLERCEPH_AWAKE_STAGE_SWELL;
                }
                break;
            case WORLD_COLLISION_CONTACT_ATTACK:
                if (work->hitCooldown == 0) {
                    attackDamage = damageComputePlayerAttack(work->contacts[contactIndex].key.value, rangeOrOverlap, 0, 0);
                    if (damageRollCriticalHit(task->spawnArg2.pointer, work->contacts[contactIndex].key.value, 0) != 0) {
                        _sucklercephKill(task, 1);
                        task->killCountdown = SUCKLERCEPH_DEATH_COUNTDOWN_FRAMES;
                        task->state         = SUCKLERCEPH_TASK_DEATH;
                        work->deathPhase    = SUCKLERCEPH_DEATH_PHASE_COUNTDOWN;
                        enemy->hp           = SUCKLERCEPH_CRITICAL_DEATH_HP;
                    } else {
                        damageAccumulateLifeDrainHp(enemy, work->contacts[contactIndex].key.value, attackDamage, 0);
                        _sucklercephTakeDamage(task, attackDamage);
                        attackReaction = damageGetPlayerAttackReaction(work->contacts[contactIndex].key.value) & SUCKLERCEPH_REACTION_LOW_MASK;
                        switch (attackReaction) {
                            case DAMAGE_PLAYER_REACTION_STAGGER:
                                work->animFrozen = 1;
                                work->awakeStage = SUCKLERCEPH_AWAKE_STAGE_SWELL;
                                break;
                            case DAMAGE_PLAYER_REACTION_POISON:
                                damageTryStartEnemyDamageOverTime(enemy, work->contacts[contactIndex].key.value, 0);
                                break;
                            case DAMAGE_PLAYER_REACTION_BUILDUP:
                            case SUCKLERCEPH_REACTION_BUILDUP_ALTERNATE:
                                damageStartEnemyBuildup(enemy, work->contacts[contactIndex].key.value, 0);
                                break;
                        }
                        if (enemy->hp > 0) {
                            effectSpawnHit(damageGetPlayerAttackEffectId(work->contacts[contactIndex].key.value), task->extra.tmd->coords + 1, NULL, &work->hitEffectArg);
                        }
                        attackCooldownFrames = damageGetPlayerAttackHitCooldown(work->contacts[contactIndex].key.value);
                        if (attackCooldownFrames > 0) {
                            work->hitCooldown = attackCooldownFrames;
                        }
                    }
                }
                break;
            case WORLD_COLLISION_CONTACT_ENEMY_BODY:
                // Contact points are view-space body centres, not wall normals.
                // Keep the overwritten range for any later attack in this table.
                bodyDx                   = rootCoord->workm.t[0] - work->contacts[contactIndex].point.vx;
                scratch->delta.vector.vy = 0;
                scratch->delta.vector.vx = bodyDx;
                bodyDz                   = rootCoord->workm.t[2] - work->contacts[contactIndex].point.vz;
                scratch->delta.vector.vz = bodyDz;
                rangeOrOverlap           = SquareRoot0((bodyDx * bodyDx) + (bodyDz * bodyDz));
                rangeOrOverlap           = work->contacts[contactIndex].distance - rangeOrOverlap;
                rangeOrOverlap           = (rangeOrOverlap <= 0) ? 0 : rangeOrOverlap;
                scratch->delta.vector.vx = rootCoord->workm.t[0] - work->contacts[contactIndex].point.vx;
                scratch->delta.vector.vy = rootCoord->workm.t[1] - work->contacts[contactIndex].point.vy;
                scratch->delta.vector.vz = rootCoord->workm.t[2] - work->contacts[contactIndex].point.vz;
                VectorNormal(&scratch->delta.vector, &scratch->normal);
                ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, &scratch->normal, &scratch->delta.vector);
                if (work->animId == SUCKLERCEPH_ANIM_IDLE || work->animId == SUCKLERCEPH_ANIM_CRAWL) {
                    rootCoord->coord.t[0] += (rangeOrOverlap * scratch->delta.vector.vx) >> SUCKLERCEPH_CONTACT_NORMAL_FRACTION_BITS;
                    verticalPushQ12        = rangeOrOverlap * scratch->delta.vector.vy;
                    if (verticalPushQ12 < 0) {
                        rootCoord->coord.t[1] += verticalPushQ12 >> SUCKLERCEPH_CONTACT_NORMAL_FRACTION_BITS;
                    }
                    rootCoord->coord.t[2] += (rangeOrOverlap * scratch->delta.vector.vz) >> SUCKLERCEPH_CONTACT_NORMAL_FRACTION_BITS;
                }
                break;
        }
    }
    worldCollisionClearContacts(work->contacts);
    attackContact = &work->attackContact;
    if ((work->awakeStage != SUCKLERCEPH_AWAKE_STAGE_NONE) && (worldCollisionFindContactIndex(attackContact, WORLD_COLLISION_FIND_ANY_KEY) != 0)) {
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        worldCollisionClearContacts(attackContact);
    }
    SCRATCH_STACK_RELEASE_BLOCK(SucklercephContactsScratch);
}
