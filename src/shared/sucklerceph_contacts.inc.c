#include "gameplay/room_effects.h"

/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Contact handler of the first enemy, with 0x4C bytes of scratch. The
/// `worldCollisionResolvePushback` push-back from its contact table moves the root (response
/// 1) or restores the position the last step started from (response 2), and
/// the hit cooldown ticks down. Coming within 0x320 of the player moves a live
/// enemy to its dying stage. Each of the four contacts is then handled by
/// class: 0x10000 does the same, 0x20000 (outside the cooldown) either kills
/// the enemy outright on a critical roll or applies the damage, the id's side
/// effect, the hit effect and the id's cooldown, and 0x30000 pushes the root
/// out of the wall along the contact normal while the enemy walks. The table
/// is released, and a flagged hit on the third body's record clears that
/// body's 0x8000 bit.
void sucklercephContacts(Task* arg0)
{
    Enemy*                      enemy;
    WorldCollisionContact*      effectRec;
    s32                         effect;
    s32                         pushY;
    s32                         movement;
    s32                         dx;
    s32                         dz;
    s32                         wallDx;
    s32                         wallDz;
    s16                         hitCooldown;
    s32                         distance;
    u32                         damage;
    SucklercephWork*            work;
    GfxCoord*                   coord;
    SucklercephContactsScratch* scratch;
    s32                         i;

    work     = arg0->work;
    scratch  = SCRATCH_STACK_RESERVE_BLOCK(SucklercephContactsScratch);
    coord    = arg0->extra.tmd->coords;
    enemy    = arg0->spawnArg2.pointer;
    movement = worldCollisionResolvePushback(work->contacts, &scratch->delta, ARRAY_SIZE(work->contacts), &scratch->gridKeyMask);
    switch (movement) {
        case 0:
            break;
        case 1:
            coord->coord.t[0] += scratch->delta.fixed.vx.halves.integer;
            coord->coord.t[1] += scratch->delta.fixed.vy.halves.integer;
            coord->coord.t[2] += scratch->delta.fixed.vz.halves.integer;
            break;
        case 2:
            coord->coord.t[0] = work->prevRootPos.vx;
            coord->coord.t[1] = work->prevRootPos.vy;
            coord->coord.t[2] = work->prevRootPos.vz;
            break;
    }
    if (work->hitCooldown != 0) {
        work->hitCooldown--;
        if (work->hitCooldown <= 0) {
            work->hitCooldown = 0;
        }
    }
    dx                       = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    scratch->delta.vector.vx = dx;
    scratch->delta.vector.vy = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
    dz                       = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    scratch->delta.vector.vz = dz;
    distance                 = SquareRoot0((dx * dx) + (dz * dz));
    if (distance < 0x320 && work->awakeStage == SUCKLERCEPH_AWAKE_STAGE_CRAWL) {
        work->animFrozen = 1;
        work->awakeStage = SUCKLERCEPH_AWAKE_STAGE_SWELL;
    }
    for (i = 0; i < ARRAY_SIZE(work->contacts); i++) {
        switch (work->contacts[i].key.value & 0xFFFF0000) {
            case 0x10000:
                if (work->awakeStage == SUCKLERCEPH_AWAKE_STAGE_CRAWL) {
                    work->animFrozen = 1;
                    work->awakeStage = SUCKLERCEPH_AWAKE_STAGE_SWELL;
                }
                break;
            case 0x20000:
                if (work->hitCooldown == 0) {
                    damage = damageComputePlayerAttack(work->contacts[i].key.value, distance, 0, 0);
                    if (damageRollCriticalHit(arg0->spawnArg2.pointer, work->contacts[i].key.value, 0) != 0) {
                        _sucklercephKill(arg0, 1);
                        arg0->killCountdown = 5;
                        arg0->state         = 2;
                        work->deathPhase    = SUCKLERCEPH_DEATH_PHASE_COUNTDOWN;
                        enemy->hp           = -1;
                    } else {
                        damageAccumulateLifeDrainHp(enemy, work->contacts[i].key.value, damage, 0);
                        sucklercephTakeDamage(arg0, damage);
                        effect = damageGetPlayerAttackReaction(work->contacts[i].key.value) & 0xFFFF;
                        switch (effect) {
                            case DAMAGE_PLAYER_REACTION_STAGGER:
                                work->animFrozen = 1;
                                work->awakeStage = SUCKLERCEPH_AWAKE_STAGE_SWELL;
                                break;
                            case DAMAGE_PLAYER_REACTION_POISON:
                                damageTryStartEnemyDamageOverTime(enemy, work->contacts[i].key.value, 0);
                                break;
                            case DAMAGE_PLAYER_REACTION_BUILDUP:
                            case 9:
                                damageStartEnemyBuildup(enemy, work->contacts[i].key.value, 0);
                                break;
                        }
                        if (enemy->hp > 0) {
                            effectSpawnHit(damageGetPlayerAttackEffectId(work->contacts[i].key.value), arg0->extra.tmd->coords + 1, NULL, &work->hitEffectArg);
                        }
                        hitCooldown = damageGetPlayerAttackHitCooldown(work->contacts[i].key.value);
                        if (hitCooldown > 0) {
                            work->hitCooldown = hitCooldown;
                        }
                    }
                }
                break;
            case 0x30000:
                wallDx                   = coord->workm.t[0] - work->contacts[i].point.vx;
                scratch->delta.vector.vy = 0;
                scratch->delta.vector.vx = wallDx;
                wallDz                   = coord->workm.t[2] - work->contacts[i].point.vz;
                scratch->delta.vector.vz = wallDz;
                distance                 = SquareRoot0((wallDx * wallDx) + (wallDz * wallDz));
                distance                 = work->contacts[i].distance - distance;
                distance                 = (distance <= 0) ? 0 : distance;
                scratch->delta.vector.vx = coord->workm.t[0] - work->contacts[i].point.vx;
                scratch->delta.vector.vy = coord->workm.t[1] - work->contacts[i].point.vy;
                scratch->delta.vector.vz = coord->workm.t[2] - work->contacts[i].point.vz;
                VectorNormal(&scratch->delta.vector, &scratch->normal);
                ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, &scratch->normal, &scratch->delta.vector);
                if (work->animId == SUCKLERCEPH_ANIM_IDLE || work->animId == SUCKLERCEPH_ANIM_CRAWL) {
                    coord->coord.t[0] += (distance * scratch->delta.vector.vx) >> 12;
                    pushY              = distance * scratch->delta.vector.vy;
                    if (pushY < 0) {
                        coord->coord.t[1] += pushY >> 12;
                    }
                    coord->coord.t[2] += (distance * scratch->delta.vector.vz) >> 12;
                }
                break;
        }
    }
    worldCollisionClearContacts(work->contacts);
    effectRec = &work->attackContact;
    if ((work->awakeStage != SUCKLERCEPH_AWAKE_STAGE_NONE) && (worldCollisionFindContactIndex(effectRec, WORLD_COLLISION_FIND_ANY_KEY) != 0)) {
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        worldCollisionClearContacts(effectRec);
    }
    SCRATCH_STACK_RELEASE_BLOCK(SucklercephContactsScratch);
}
