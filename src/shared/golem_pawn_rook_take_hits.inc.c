#include "main/random.h"

#include "gameplay/room_effects.h"

/* Part of the Pawn/Rook GOLEM library; see golem_pawn_rook.h. */

/// Carrier tables mark which attack rows a raised frontal shield absorbs.
///
/// Each binding is an s16 array indexed by the attack key's low seven bits;
/// the PE table is selected by bit 15. The table spellings are carrier-owned.
#if GOLEM_PAWN_ROOK_WEAPON == GOLEM_GRENADE_LAUNCHER
#define GOLEM_PAWN_ROOK_SHIELD_WEAPON_ROWS gGolemPawnRookWeakPointWeapons
#define GOLEM_PAWN_ROOK_SHIELD_PE_ROWS     gGolemPawnRookWeakPointPe
#else
#define GOLEM_PAWN_ROOK_SHIELD_WEAPON_ROWS gGolemPawnRookWeakSpotHits
#define GOLEM_PAWN_ROOK_SHIELD_PE_ROWS     gGolemPawnRookWeakSpotHitsFlagged
#endif

/// Ends a hit-interrupted scream effect and clears the body's borrowed handle.
///
/// work and its non-NULL screamEffect must be live; ownership stays with the effect task.
static inline void _golemPawnRookEndHitScream(GolemPawnRookWork* work)
{
    enum { GOLEM_PAWN_ROOK_HIT_SCREAM_END_STATE = 3 };
    work->screamEffect->task->state = GOLEM_PAWN_ROOK_HIT_SCREAM_END_STATE;
    work->screamEffect              = NULL;
}

/// Resolves body contacts, shield and HP damage, hit reactions and player sight.
///
/// actor owns live GOLEM work, its Enemy record, and initialized contact tables.
/// Attack keys select valid carrier shield-table rows and the attacker task via
/// bit 7; no row checks run. Ground corrections include Y, hurt-body corrections
/// use X/Z, and the deepest enemy-body overlap supplies the final room-axis push.
/// The reaction and shield-absorption decisions are retained across the contact
/// scan. Only a change from the last hit key spawns another hit effect. Contacts
/// are consumed here, and scratch is released before returning. Sight uses the
/// player's composed part 4 and the composed body root without retaining points.
static void _golemPawnRookTakeHits(Task* actor)
{
    enum {
        GOLEM_PAWN_ROOK_HIT_FLINCH                  = 0,
        GOLEM_PAWN_ROOK_HIT_STAGGER                 = 1,
        GOLEM_PAWN_ROOK_HIT_RECOIL                  = 2,
        GOLEM_PAWN_ROOK_HIT_KNOCKDOWN               = 3,
        GOLEM_PAWN_ROOK_HIT_DOWNED                  = 4,
        GOLEM_PAWN_ROOK_HIT_COLLAPSE                = 5,
        GOLEM_PAWN_ROOK_HIT_DOWNED_DEATH            = 6,
        GOLEM_PAWN_ROOK_HIT_PE_BIT                  = 0x8000,
        GOLEM_PAWN_ROOK_HIT_ATTACKER_SHIFT          = 7,
        GOLEM_PAWN_ROOK_HIT_ROW_MASK                = 0x7F,
        GOLEM_PAWN_ROOK_HIT_DOUBLE_DAMAGE_ATTRIBUTE = 5,
        GOLEM_PAWN_ROOK_HIT_DOUBLE_DAMAGE_STYLE     = 2, // cyan spiked critical-hit burst
        GOLEM_PAWN_ROOK_HIT_CRITICAL_STYLE          = 0, // yellow spiked critical-hit burst
        GOLEM_PAWN_ROOK_HIT_FRONT_EFFECT_Z          = 300,
        GOLEM_PAWN_ROOK_HIT_BEHIND_EFFECT_Z         = -150,
        GOLEM_PAWN_ROOK_HIT_NORMAL_FRACTION_BITS    = 12,

    };

    s32                      hitReaction;
    s32                      deepestPush;
    s32                      shieldAbsorbed;
    u32                      lastEffectKey;
    GolemPawnRookWork*       work;
    GolemPawnRookHitScratch* scratchEnd;
    GolemPawnRookHitScratch* scratch;
    Enemy*                   enemy;
    GfxCoord*                root;
    GfxCoord*                attackerRoot;
    GfxCoord*                bodyPart;
    s32                      contactIndex;
    s32                      offsetX, offsetY, offsetZ;
    s32                      damage;
    s32                      attackReaction;
    s32                      attackerOffsetZ;
    s32                      positivePush;
    s32                      reactionValue; // forward-axis dot, then signed random yaw tilt
    s32                      overlapDepth;
    s16                      hitCooldownFrames;
    u32                      randomBits;
    s32                      pitchTilt;
    s32                      yawRandomBits;
    s32                      hpMax;

    hitReaction    = GOLEM_PAWN_ROOK_HIT_FLINCH;
    deepestPush    = 0;
    shieldAbsorbed = 0;
    lastEffectKey  = 0;
    work           = actor->work;
    scratchEnd     = SCRATCH_STACK_CURSOR(GolemPawnRookHitScratch);
    root           = actor->extra.tmd->coords;
    SCRATCH_STACK_RESERVE_BLOCK(GolemPawnRookHitScratch);
    scratch = SCRATCH_STACK_CURSOR(GolemPawnRookHitScratch);
    enemy   = actor->spawnArg2.pointer;

    // Resolve grid correction before consuming weapon hits and body overlaps.
    switch (worldCollisionResolvePushback(work->groundContacts, &scratchEnd[-1].delta, ARRAY_SIZE(work->groundContacts), NULL)) {
        case WORLD_COLLISION_PUSHBACK_NO_GRID_HIT:
            break;
        case WORLD_COLLISION_PUSHBACK_GRID_HIT:
            root->coord.t[0] += scratchEnd[-1].delta.fixed.vx.halves.integer;
            root->coord.t[1] += scratch->delta.fixed.vy.halves.integer;
            root->coord.t[2] += scratch->delta.fixed.vz.halves.integer;
            break;
        case WORLD_COLLISION_PUSHBACK_OPPOSED:
            root->coord.t[0] = work->prevRootPos.vx;
            root->coord.t[1] = work->prevRootPos.vy;
            root->coord.t[2] = work->prevRootPos.vz;
            break;
    }
    worldCollisionClearContacts(work->groundContacts);

    if (work->hurtBody.flags & WORLD_COLLISION_BODY_GRID_ENABLED) {
        switch (worldCollisionResolvePushback(work->hurtContacts, &scratch->delta, ARRAY_SIZE(work->hurtContacts), NULL)) {
            case WORLD_COLLISION_PUSHBACK_NO_GRID_HIT:
                break;
            case WORLD_COLLISION_PUSHBACK_GRID_HIT:
                root->coord.t[0] += scratch->delta.fixed.vx.halves.integer;
                root->coord.t[2] += scratch->delta.fixed.vz.halves.integer;
                break;
            case WORLD_COLLISION_PUSHBACK_OPPOSED:
                root->coord.t[0] = work->prevRootPos.vx;
                root->coord.t[2] = work->prevRootPos.vz;
                break;
        }
    }

    if (work->hitCooldown != 0) {
        if (--work->hitCooldown <= 0) {
            work->hitCooldown = 0;
        }
    }

    for (contactIndex = 0; contactIndex < ARRAY_SIZE(work->hurtContacts); contactIndex++) {
        switch ((u32)work->hurtContacts[contactIndex].key.value >> 16) {
            case 0:
            case WORLD_COLLISION_CONTACT_PLAYER_BODY >> 16:
                break;
            case WORLD_COLLISION_CONTACT_ATTACK >> 16:
                if (work->hitCooldown != 0) {
                    break;
                }
                attackerRoot             = gPlayerActorTasks[((u32)work->hurtContacts[contactIndex].key.value >> GOLEM_PAWN_ROOK_HIT_ATTACKER_SHIFT) & 1]->extra.tmd->coords;
                scratch->delta.vector.vx = attackerRoot->coord.t[0] - root->coord.t[0];
                scratch->delta.vector.vy = attackerRoot->coord.t[1] - root->coord.t[1];
                attackerOffsetZ          = attackerRoot->coord.t[2] - root->coord.t[2];
                scratch->delta.vector.vz = attackerOffsetZ;
                reactionValue            = (scratch->delta.vector.vx * root->coord.m[0][2]) + (scratch->delta.vector.vy * root->coord.m[1][2]) + (attackerOffsetZ * root->coord.m[2][2]);
                work->hitFromFront       = reactionValue >= 0;
                damage                   = damageComputePlayerAttack(work->hurtContacts[contactIndex].key.value,
                                                                     SquareRoot0((scratch->delta.vector.vx * scratch->delta.vector.vx) + (scratch->delta.vector.vy * scratch->delta.vector.vy) + (scratch->delta.vector.vz * scratch->delta.vector.vz)),
                                                                     0, 0);
                attackReaction           = damageGetPlayerAttackReaction(work->hurtContacts[contactIndex].key.value);
                if (work->shieldRaised != 0 && work->hitFromFront == 1 && work->downedPose == 0) {
                    if (work->hurtContacts[contactIndex].key.value & GOLEM_PAWN_ROOK_HIT_PE_BIT) {
                        if (GOLEM_PAWN_ROOK_SHIELD_PE_ROWS[work->hurtContacts[contactIndex].key.value & GOLEM_PAWN_ROOK_HIT_ROW_MASK] != 0) {
                            shieldAbsorbed  = 1;
                            work->shieldHp -= damage;
                        }
                    } else if (GOLEM_PAWN_ROOK_SHIELD_WEAPON_ROWS[work->hurtContacts[contactIndex].key.value & GOLEM_PAWN_ROOK_HIT_ROW_MASK] != 0) {
                        shieldAbsorbed  = 1;
                        work->shieldHp -= damage;
                    }
                    if (shieldAbsorbed == 1) {
                        if (work->shieldHp <= 0) {
                            work->behavior          = GOLEM_PAWN_ROOK_BEHAVIOR_RECOIL;
                            work->shieldRaised      = 0;
                            work->shieldBreakStep   = 1;
                            work->step              = GOLEM_PAWN_ROOK_BEHAVIOR_START_STEP;
                            work->strikeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                            if (work->screamEffect != NULL) {
                                _golemPawnRookEndHitScream(work);
                            }
                        }
                        worldTargetAddReadoutAmount(&enemy->node, 0, 0);
                        hitCooldownFrames = damageGetPlayerAttackHitCooldown(work->hurtContacts[contactIndex].key.value);
                        if (hitCooldownFrames > 0) {
                            work->hitCooldown = hitCooldownFrames;
                        }
                        break;
                    }
                } else {
                    work->shieldRaised = 0;
                    if ((attackReaction & 0xFFFF) == GOLEM_PAWN_ROOK_HIT_DOUBLE_DAMAGE_ATTRIBUTE) {
                        damage *= 2;
                        effectSpawn(EFFECT_CRITICAL_HIT, &actor->extra.tmd->coords[3], GOLEM_PAWN_ROOK_HIT_DOUBLE_DAMAGE_STYLE, NULL);
                    }
                }
                if (damageRollCriticalHit(enemy, work->hurtContacts[contactIndex].key.value, 0) != 0) {
                    damage *= 4;
                    if ((attackReaction & 0xFFFF) != GOLEM_PAWN_ROOK_HIT_DOUBLE_DAMAGE_ATTRIBUTE) {
                        effectSpawn(EFFECT_CRITICAL_HIT, &actor->extra.tmd->coords[3], GOLEM_PAWN_ROOK_HIT_CRITICAL_STYLE, NULL);
                    }
                    if (work->buildupActive == 0) {
                        hitReaction = GOLEM_PAWN_ROOK_HIT_STAGGER;
                    }
                }
                if (work->screamCharges != 0 && (work->hurtContacts[contactIndex].key.value & GOLEM_PAWN_ROOK_HIT_PE_BIT)) {
                    damage >>= 2;
                }
                worldTargetAddReadoutAmount(&enemy->node, damage, 0);
                damageAccumulateLifeDrainHp(enemy, work->hurtContacts[contactIndex].key.value, damage, 0);
                enemy->hp -= damage;
                if (enemy->hp <= 0) {
                    if (work->downedPose == 0) {
                        hitReaction = GOLEM_PAWN_ROOK_HIT_COLLAPSE;
                    } else {
                        hitReaction = GOLEM_PAWN_ROOK_HIT_DOWNED_DEATH;
                    }
                } else if (hpMax = enemy->param->hpMax, enemy->hp < GOLEM_PAWN_ROOK_LOW_HP(hpMax)) {
                    if (work->downedPose == 0) {
                        hitReaction = GOLEM_PAWN_ROOK_HIT_KNOCKDOWN;
                    } else {
                        hitReaction = GOLEM_PAWN_ROOK_HIT_DOWNED;
                    }
                }
                if (work->attackActive != 0 || work->screamActive != 0) {
                    work->interruptDamage += damage;
                }
                switch (attackReaction & 0xFFFF) {
                    case DAMAGE_PLAYER_REACTION_STAGGER:
                        if (work->screamCharges == 0 && work->downedPose == 0 && hitReaction < GOLEM_PAWN_ROOK_HIT_KNOCKDOWN && work->buildupActive == 0) {
                            hitReaction = GOLEM_PAWN_ROOK_HIT_RECOIL;
                        }
                        break;
                    case DAMAGE_PLAYER_REACTION_BUILDUP:
                        if (work->screamCharges == 0 && work->downedPose == 0 && hitReaction < GOLEM_PAWN_ROOK_HIT_KNOCKDOWN) {
                            damageStartEnemyBuildup(enemy, work->hurtContacts[contactIndex].key.value, 0);
                            hitReaction = GOLEM_PAWN_ROOK_HIT_STAGGER;
                        }
                        break;
                    case DAMAGE_PLAYER_REACTION_NONE:
                    case DAMAGE_PLAYER_REACTION_POISON:
                    case 4:
                    case GOLEM_PAWN_ROOK_HIT_DOUBLE_DAMAGE_ATTRIBUTE:
                    case DAMAGE_PLAYER_REACTION_EXPLOSION:
                    case DAMAGE_PLAYER_REACTION_INCENDIARY:
                    case 8:
                    case 9:
                        break;
                }
                if (lastEffectKey != work->hurtContacts[contactIndex].key.value) {
                    lastEffectKey            = work->hurtContacts[contactIndex].key.value;
                    scratch->effectOffset.vx = 0;
                    scratch->effectOffset.vy = 0;
                    scratch->effectOffset.vz = (work->hitFromFront == 1) ? GOLEM_PAWN_ROOK_HIT_FRONT_EFFECT_Z : GOLEM_PAWN_ROOK_HIT_BEHIND_EFFECT_Z;
                    effectSpawnHit(damageGetPlayerAttackEffectId(work->hurtContacts[contactIndex].key.value), &actor->extra.tmd->coords[3],
                                   &scratch->effectOffset, &work->hitEffectArg);
                }
                hitCooldownFrames = damageGetPlayerAttackHitCooldown(work->hurtContacts[contactIndex].key.value);
                if (hitCooldownFrames > 0) {
                    work->hitCooldown = hitCooldownFrames;
                }
                switch (hitReaction) {
                    case GOLEM_PAWN_ROOK_HIT_FLINCH:
                        if (work->behavior < GOLEM_PAWN_ROOK_BEHAVIOR_ENGAGE) {
                            work->anim     = GOLEM_PAWN_ROOK_ANIM_WALK;
                            work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_ENGAGE;
                            work->step     = GOLEM_PAWN_ROOK_BEHAVIOR_START_STEP;
                        }
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        randomBits      = gRandomLcgState >> 16;
                        pitchTilt       = (randomBits & 0x7F) + 0x40;
                        if (!(randomBits & 1)) {
                            pitchTilt = -pitchTilt;
                        }
                        work->hitTilt.vx = pitchTilt;
                        yawRandomBits    = (s16)randomBits >> 8;
                        reactionValue    = (yawRandomBits & 0x7F) + 0x40;
                        if (!(yawRandomBits & 1)) {
                            reactionValue = -reactionValue;
                        }
                        work->hitTilt.vy    = reactionValue;
                        work->hitTiltActive = 1;
                        break;
                    case GOLEM_PAWN_ROOK_HIT_STAGGER:
                        work->behavior          = GOLEM_PAWN_ROOK_BEHAVIOR_STAGGER;
                        work->step              = GOLEM_PAWN_ROOK_BEHAVIOR_START_STEP;
                        work->strikeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        break;
                    case GOLEM_PAWN_ROOK_HIT_RECOIL:
                        work->behavior          = GOLEM_PAWN_ROOK_BEHAVIOR_RECOIL;
                        work->step              = GOLEM_PAWN_ROOK_BEHAVIOR_START_STEP;
                        work->strikeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        break;
                    case GOLEM_PAWN_ROOK_HIT_KNOCKDOWN:
                        work->behavior          = GOLEM_PAWN_ROOK_BEHAVIOR_KNOCKDOWN;
                        work->step              = GOLEM_PAWN_ROOK_BEHAVIOR_START_STEP;
                        work->strikeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        break;
                    case GOLEM_PAWN_ROOK_HIT_DOWNED:
                        if (work->fallingDown == 0) {
                            work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_DOWNED_HIT;
                            work->step     = GOLEM_PAWN_ROOK_BEHAVIOR_START_STEP;
                        }
                        break;
                    case GOLEM_PAWN_ROOK_HIT_COLLAPSE:
                        work->behavior          = GOLEM_PAWN_ROOK_BEHAVIOR_COLLAPSE;
                        work->step              = GOLEM_PAWN_ROOK_BEHAVIOR_START_STEP;
                        work->strikeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        break;
                    case GOLEM_PAWN_ROOK_HIT_DOWNED_DEATH:
                        if (work->fallingDown == 0) {
                            work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_DOWNED_DEATH;
                            work->step     = GOLEM_PAWN_ROOK_BEHAVIOR_START_STEP;
                        }
                        break;
                }
                if (hitReaction != GOLEM_PAWN_ROOK_HIT_FLINCH && work->screamEffect != NULL) {
                    _golemPawnRookEndHitScream(work);
                }
                break;
            case WORLD_COLLISION_CONTACT_ENEMY_BODY >> 16:
                bodyPart                 = &actor->extra.tmd->coords[3];
                offsetX                  = bodyPart->workm.t[0] - work->hurtContacts[contactIndex].point.vx;
                scratch->delta.vector.vx = offsetX;
                offsetY                  = bodyPart->workm.t[1] - work->hurtContacts[contactIndex].point.vy;
                scratch->delta.vector.vy = offsetY;
                offsetZ                  = bodyPart->workm.t[2] - work->hurtContacts[contactIndex].point.vz;
                scratch->delta.vector.vz = offsetZ;
                overlapDepth             = work->hurtContacts[contactIndex].distance - SquareRoot0((offsetX * offsetX) + (offsetY * offsetY) + (offsetZ * offsetZ));
                positivePush             = overlapDepth;
                if (overlapDepth <= 0) {
                    positivePush = 0;
                }
                overlapDepth = positivePush;
                if (deepestPush < overlapDepth) {
                    deepestPush = overlapDepth;
                    VectorNormal(&scratch->delta.vector, &scratch->normal);
                    ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, &scratch->normal, &scratch->pushDirection);
                }
                break;
        }
    }

    // Apply the deepest body overlap, then refresh player visibility.
    if (deepestPush > 0) {
        root->coord.t[0] += (deepestPush * scratch->pushDirection.vx) >> GOLEM_PAWN_ROOK_HIT_NORMAL_FRACTION_BITS;
        root->coord.t[2] += (deepestPush * scratch->pushDirection.vz) >> GOLEM_PAWN_ROOK_HIT_NORMAL_FRACTION_BITS;
    }
    worldCollisionClearContacts(work->hurtContacts);
    if (work->strikeContacts[0].flags & WORLD_COLLISION_CONTACT_OCCUPIED) {
        work->strikeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        worldCollisionClearContacts(work->strikeContacts);
    }
    work->playerSpotted = 0;
    if (worldCollisionCountContactsByKind(work->sightContacts, WORLD_COLLISION_CONTACT_PLAYER_BODY) != 0) {
        bodyPart                 = &(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords[4];
        scratch->effectOffset.vx = bodyPart->workm.t[0];
        scratch->effectOffset.vy = bodyPart->workm.t[1];
        scratch->effectOffset.vz = bodyPart->workm.t[2];
        scratch->rootPos.vx      = root->workm.t[0];
        scratch->rootPos.vy      = root->workm.t[1];
        scratch->rootPos.vz      = root->workm.t[2];
        if (_playerDetectionSegmentOccluded(&scratch->effectOffset, &scratch->rootPos) == 0) {
            work->playerSpotted = 1;
        }
    }
    worldCollisionClearContacts(work->sightContacts);
    SCRATCH_STACK_RELEASE_BLOCK(GolemPawnRookHitScratch);
}

#undef GOLEM_PAWN_ROOK_SHIELD_WEAPON_ROWS
#undef GOLEM_PAWN_ROOK_SHIELD_PE_ROWS
