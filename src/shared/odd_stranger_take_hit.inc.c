/* Part of the Odd Stranger library; see odd_stranger.h. */

/// Applies the hit-bearing recoil before calculating distance-based damage.
///
/// Recent damage shortens the step from 100 to 25 units. The normalized Q12
/// axis is GTE-scaled and added to XYZ local translation, then marked dirty.
static inline void _oddStrangerApplyHitRecoil(Task* task, OddStrangerWork* work, ActorHitScratch* hitScratch)
{
    enum { ODD_STRANGER_HIT_RECENT_RECOIL = 25,
           ODD_STRANGER_HIT_RECOIL        = 100 };
    SVECTOR* knockback;
    hitScratch->towardHit = task->extra.tmd->coords->coord;
    gfxRotMatrixY(&hitScratch->towardHit, hitScratch->hitYaw, GRAPHICS_ROTATION_COMPOSE);
    knockback = &hitScratch->hitOffset;
    gfxReadMatrixZAxis(&hitScratch->towardHit, knockback);
    VectorNormalSS(knockback, knockback);
    if (work->recentDamageTimer > 0) {
        gte_lddp(-ODD_STRANGER_HIT_RECENT_RECOIL);
        gte_ldsv(knockback);
        gte_gpf12();
        gte_stsv(knockback);
    } else {
        gte_lddp(-ODD_STRANGER_HIT_RECOIL);
        gte_ldsv(knockback);
        gte_gpf12();
        gte_stsv(knockback);
    }
    task->extra.tmd->coords->coord.t[0]  += hitScratch->hitOffset.vx;
    task->extra.tmd->coords->coord.t[1]  += hitScratch->hitOffset.vy;
    task->extra.tmd->coords->coord.t[2]  += hitScratch->hitOffset.vz;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Consumes a live Odd Stranger's attacks, applies damage and selects hit reactions.
///
/// Requires initialized actor/enemy/model/rig and the live player. While HP is
/// positive, reserves one hit scratch block and scans hit contacts (then grid
/// contacts in variant 2), stopping at the first attack key. Contacts remain
/// borrowed and uncleared. Attachment attacks use the player's composed origin
/// instead of their contact point. Hit bearings use the model's composition
/// frame; recoil is applied to local translation as in the original.
///
/// Damage uses player distance, quadruples a rolled critical, and doubles a rear
/// hit beyond 1280/4096 turn outside downed/status poses. Hit effects/sounds,
/// life drain, readout, five-tick recent damage, buildup and damage over time
/// retain their order. Reaction/animation and fatal-damage paths differ by
/// carrier variant. Held-player release is synchronous. The scratch reservation
/// is released before return; an actor already at zero HP is left untouched.
static void _oddStrangerTakeHit(Task* task)
{
    enum {
        ODD_STRANGER_HIT_ATTACHMENT_BIT           = 0x8000,
        ODD_STRANGER_HIT_REACTION_MASK            = 0xFFFF,
        ODD_STRANGER_HIT_REACTION_FALL            = 4,
        ODD_STRANGER_HIT_REACTION_STANDARD        = 5,
        ODD_STRANGER_HIT_REACTION_FRONT_FALL      = 8,
        ODD_STRANGER_HIT_REACTION_RESTART_FALL    = 9,
        ODD_STRANGER_HIT_BEHIND_YAW_MIN           = 0x501,
        ODD_STRANGER_HIT_FALL_YAW_LIMIT           = ACTOR_TRANSFORM_ANGLE_TURN / 4,
        ODD_STRANGER_HIT_RECENT_FRAMES            = 5,
        ODD_STRANGER_HIT_RISE_FRONT_REFALL_FRAMES = 33,
        ODD_STRANGER_HIT_RISE_BACK_REFALL_FRAMES  = 12,
        ODD_STRANGER_CRITICAL_EFFECT_NONE         = -1,
        ODD_STRANGER_CRITICAL_EFFECT_ROLLED       = 0,
        ODD_STRANGER_CRITICAL_EFFECT_BEHIND       = 4,
        ODD_STRANGER_HIT_ANIM_FALL_BACK           = 10,
        ODD_STRANGER_SOUND_HURT                   = SOUND_ACTOR_311500_HURT,
        ODD_STRANGER_SOUND_DEATH                  = SOUND_ACTOR_311500_DEATH
    };
    PlayerStatus*    playerStatus = &gPlayerStatus;
    OddStrangerWork* work;
    Enemy*           enemy;
    ActorHitScratch* scratchHead;
    ActorHitScratch* hitScratch;
    GfxCoord*        rootCoord;
    Task*            player;
    s16              hitOffsetZ;
    s32              hitBearing;
    s32 // Apply distance damage, critical/rear bonuses and readout before reaction.
        playerDeltaX;
    s32 playerDeltaY;
    s32 playerDeltaZ;
    s32 deathSound;
    s32 deathPan;
    s32 hitSound;
    s32 hitPan;
    s32 absHitYaw;
#if ODD_STRANGER_VARIANT == 2
    s32 stateOrHitKey; // Variant 2 reuses this word for narrowed state tests and fatal-hit classification
#endif
    s16 state;
#if ODD_STRANGER_VARIANT == 2
    s16 deathAnim;
#endif
    s16 criticalEffectKind;
    u32 doubledDamage;

    enemy = task->spawnArg2.pointer;
    work  = task->work;
    // Resolve contact origin and release any held player before recoil/damage.
    if (enemy->hp > 0) {
        scratchHead        = SCRATCH_STACK_CURSOR(ActorHitScratch);
        hitScratch         = (SCRATCH_STACK_CURSOR(ActorHitScratch) = scratchHead - 1);
        hitScratch->hitKey = _actorContactFindAttack(&scratchHead[-1].hitPos, work->hitContacts, ARRAY_SIZE(work->hitContacts));
#if ODD_STRANGER_VARIANT == 2
        if (hitScratch->hitKey == 0) {
            hitScratch->hitKey = _actorContactFindAttack(&hitScratch->hitPos, work->gridContacts, ARRAY_SIZE(work->gridContacts));
        }
#endif
        if (hitScratch->hitKey != 0) {
            if (hitScratch->hitKey & ODD_STRANGER_HIT_ATTACHMENT_BIT) {
                player                = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                hitScratch->hitPos.vx = player->extra.tmd->coords->workm.t[0];
                hitScratch->hitPos.vy = player->extra.tmd->coords->workm.t[1];
                hitScratch->hitPos.vz = player->extra.tmd->coords->workm.t[2];
            }
            if (work->playerHeld == 1) {
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                work->playerHeld = 0;
                if (work->state == ODD_STRANGER_STATE_GRAB || work->state == ODD_STRANGER_STATE_GRAB_PULL || work->state == ODD_STRANGER_STATE_GRAB_STRIKE || work->state == ODD_STRANGER_STATE_GRAB_RELEASE) {
                    work->state = ODD_STRANGER_STATE_FALL_BACK;
                }
            }
            work->dashCount                       = 0;
            work->sidestepCount                   = 0;
            task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(task->extra.tmd->coords);
            hitScratch->hitOffset.vx = task->extra.tmd->coords->workm.t[0];
            hitScratch->hitOffset.vy = task->extra.tmd->coords->workm.t[1];
            hitScratch->hitOffset.vz = task->extra.tmd->coords->workm.t[2];
            hitScratch->hitOffset.vx = hitScratch->hitPos.vx - task->extra.tmd->coords->workm.t[0];
            hitScratch->hitOffset.vy = hitScratch->hitPos.vy - task->extra.tmd->coords->workm.t[1];
            hitOffsetZ               = hitScratch->hitPos.vz - task->extra.tmd->coords->workm.t[2];
            hitScratch->hitOffset.vz = hitOffsetZ;
            hitBearing               = ratan2(hitScratch->hitOffset.vx, hitOffsetZ);
            rootCoord                = task->extra.tmd->coords;
            hitScratch->hitYaw       = hitBearing - ratan2(-rootCoord->workm.m[2][0], rootCoord->workm.m[2][2]);
            hitScratch->hitYaw       = _actorAngleNormalizeYaw(hitScratch->hitYaw);
            _oddStrangerSpawnHitEffect(task, hitScratch->hitYaw, hitScratch->hitKey);
            work->lookYaw              = 0;
            work->lookYawTarget        = 0;
            hitScratch->criticalEffect = ODD_STRANGER_CRITICAL_EFFECT_NONE;
            state                      = work->state;
            if (state != ODD_STRANGER_STATE_FALL_BACK && state != ODD_STRANGER_STATE_FALL_FRONT && state != ODD_STRANGER_STATE_DOWN && state != ODD_STRANGER_STATE_REFALL_BACK && state != ODD_STRANGER_STATE_REFALL_FRONT && state != ODD_STRANGER_STATE_RISE_BACK && state != ODD_STRANGER_STATE_RISE_FRONT && state != ODD_STRANGER_STATE_STATUS_HOLD) {
                _oddStrangerApplyHitRecoil(task, work, hitScratch);
            }
            // Apply distance damage, critical/rear bonuses and readout before reaction.
            playerDeltaX               = playerStatus->coordMtx->t[0] - task->extra.tmd->coords->coord.t[0];
            hitScratch->toPlayer.vx    = playerDeltaX;
            playerDeltaY               = playerStatus->coordMtx->t[1] - task->extra.tmd->coords->coord.t[1];
            hitScratch->toPlayer.vy    = playerDeltaY;
            playerDeltaZ               = playerStatus->coordMtx->t[2] - task->extra.tmd->coords->coord.t[2];
            hitScratch->toPlayer.vz    = playerDeltaZ;
            hitScratch->playerDistance = SquareRoot0(playerDeltaX * playerDeltaX + playerDeltaY * playerDeltaY + playerDeltaZ * playerDeltaZ);
            hitScratch->damage         = damageComputePlayerAttack(hitScratch->hitKey, hitScratch->playerDistance, 0, 0);
            if (damageRollCriticalHit(enemy, hitScratch->hitKey, 0) != 0) {
                hitScratch->critical       = 1;
                hitScratch->criticalEffect = ODD_STRANGER_CRITICAL_EFFECT_ROLLED;
                hitScratch->damage        *= 4;
            } else {
                hitScratch->critical = 0;
            }
            absHitYaw = hitScratch->hitYaw;
            if (absHitYaw < 0) {
                absHitYaw = -absHitYaw;
            }
            if (absHitYaw >= ODD_STRANGER_HIT_BEHIND_YAW_MIN) {
                state = work->state;
                if (state != ODD_STRANGER_STATE_FALL_BACK) {
                    if (state != ODD_STRANGER_STATE_FALL_FRONT && state != ODD_STRANGER_STATE_DOWN && state != ODD_STRANGER_STATE_REFALL_BACK && state != ODD_STRANGER_STATE_REFALL_FRONT && state != ODD_STRANGER_STATE_RISE_BACK && state != ODD_STRANGER_STATE_RISE_FRONT && state != ODD_STRANGER_STATE_STATUS_HOLD) {
                        doubledDamage      = hitScratch->damage * 2;
                        hitScratch->damage = doubledDamage;
                        if (doubledDamage != 0) {
                            hitScratch->criticalEffect = ODD_STRANGER_CRITICAL_EFFECT_BEHIND;
                        }
                    }
                }
            }
            damageAccumulateLifeDrainHp(enemy, hitScratch->hitKey, hitScratch->damage, 0);
            enemy->hp -= hitScratch->damage;
            worldTargetAddReadoutAmount(&enemy->node, hitScratch->damage, 0);
            work->recentDamage += hitScratch->damage;
            criticalEffectKind  = hitScratch->criticalEffect;
            if (criticalEffectKind != ODD_STRANGER_CRITICAL_EFFECT_NONE) {
                effectSpawn(EFFECT_CRITICAL_HIT, &task->extra.tmd->coords[2], (s32)(criticalEffectKind), NULL);
            }
            if (work->state == ODD_STRANGER_STATE_DORMANT_SCRIPTED) {
                sndEvtRequestScriptStop(SOUND_ACROPOLIS_PATIO_STRANGER_DORMANT, SOUND_SCRIPT_STOP_KEEP_RELEASE);
            }
            if ((work->state == ODD_STRANGER_STATE_GRAB_PULL || work->state == ODD_STRANGER_STATE_GRAB_STRIKE || work->state == ODD_STRANGER_STATE_GRAB_RELEASE) && playerStatus->hp > 0 && work->playerHeld == 1) {
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
            }
            if (enemy->hp <= 0) {
                deathSound = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ODD_STRANGER_SOUND_DEATH;
                deathPan   = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
                sndEvtRequestScriptStart(deathSound, deathPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
            } else {
                hitSound = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ODD_STRANGER_SOUND_HURT;
                hitPan   = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
                sndEvtRequestScriptStart(hitSound, hitPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
            }
            work->hitCooldown = damageGetPlayerAttackHitCooldown(hitScratch->hitKey);
            switch (damageGetPlayerAttackReaction(hitScratch->hitKey) & ODD_STRANGER_HIT_REACTION_MASK) {
                case ODD_STRANGER_HIT_REACTION_FALL:
                    state = work->state;
                    if (state != ODD_STRANGER_STATE_FALL_BACK && state != ODD_STRANGER_STATE_FALL_FRONT && state != ODD_STRANGER_STATE_REFALL_BACK && state != ODD_STRANGER_STATE_REFALL_FRONT
#if ODD_STRANGER_VARIANT == 1
                        && state != ODD_STRANGER_STATE_STATUS_HOLD
#endif
                        && state != ODD_STRANGER_STATE_DOWN) {
                        if (work->state == ODD_STRANGER_STATE_RISE_FRONT && work->stateTimer < ODD_STRANGER_HIT_RISE_FRONT_REFALL_FRAMES) {
                            work->state = ODD_STRANGER_STATE_REFALL_FRONT;
                        } else if (work->state == ODD_STRANGER_STATE_RISE_BACK && work->stateTimer < ODD_STRANGER_HIT_RISE_BACK_REFALL_FRAMES) {
                            work->state = ODD_STRANGER_STATE_REFALL_BACK;
                        } else {
                            absHitYaw = hitScratch->hitYaw;
                            if (absHitYaw < 0) {
                                absHitYaw = -absHitYaw;
                            }
                            work->state = (absHitYaw < ODD_STRANGER_HIT_FALL_YAW_LIMIT) ? ODD_STRANGER_STATE_FALL_BACK : ODD_STRANGER_STATE_FALL_FRONT;
                        }
                    }
                    break;
                case DAMAGE_PLAYER_REACTION_NONE:
                case ODD_STRANGER_HIT_REACTION_STANDARD:
                case DAMAGE_PLAYER_REACTION_EXPLOSION:
                case DAMAGE_PLAYER_REACTION_INCENDIARY:
#if ODD_STRANGER_VARIANT == 2
                    if (work->state == ODD_STRANGER_STATE_PATROL || work->state == ODD_STRANGER_STATE_DORMANT || work->state == ODD_STRANGER_STATE_DORMANT_SCRIPTED) {
                        work->state = ODD_STRANGER_STATE_ALERT;
                    }
#endif
                    state = work->state;
                    if (state == ODD_STRANGER_STATE_FALL_BACK || state == ODD_STRANGER_STATE_FALL_FRONT || state == ODD_STRANGER_STATE_RISE_BACK || state == ODD_STRANGER_STATE_RISE_FRONT || state == ODD_STRANGER_STATE_STATUS_HOLD || state == ODD_STRANGER_STATE_DOWN) {
                        if (work->animId == ODD_STRANGER_ANIM_DOWN_BACK || work->animId == ODD_STRANGER_ANIM_STATUS_BACK || work->animId == ODD_STRANGER_ANIM_RISE_BACK || work->animId == ODD_STRANGER_HIT_ANIM_FALL_BACK) {
                            work->blendActive = 1;
                            work->blendAnimId = ODD_STRANGER_ANIM_DOWN_BACK;
                        } else {
                            work->blendActive = 1;
                            work->blendAnimId = ODD_STRANGER_ANIM_REFALL_FRONT;
                        }
                        work->blendRequest = ODD_STRANGER_ANIM_REQUEST_RESET;
                    } else if (work->recentDamage >= ODD_STRANGER_STAGGER_DAMAGE || hitScratch->critical == 1) {
                        if (work->state == ODD_STRANGER_STATE_RISE_FRONT && work->stateTimer < ODD_STRANGER_HIT_RISE_FRONT_REFALL_FRAMES) {
                            work->state = ODD_STRANGER_STATE_REFALL_FRONT;
                        } else if (work->state == ODD_STRANGER_STATE_RISE_BACK && work->stateTimer < ODD_STRANGER_HIT_RISE_BACK_REFALL_FRAMES) {
                            work->state = ODD_STRANGER_STATE_REFALL_BACK;
                        } else {
                            absHitYaw = hitScratch->hitYaw;
                            if (absHitYaw < 0) {
                                absHitYaw = -absHitYaw;
                            }
                            work->state = (absHitYaw < ODD_STRANGER_HIT_FALL_YAW_LIMIT) ? ODD_STRANGER_STATE_FALL_BACK : ODD_STRANGER_STATE_FALL_FRONT;
                        }
                    } else {
                        work->blendAnimId  = ODD_STRANGER_ANIM_FLINCH;
                        work->blendActive  = 1;
                        work->blendRequest = ODD_STRANGER_ANIM_REQUEST_RESET;
                    }
                    break;
                case DAMAGE_PLAYER_REACTION_BUILDUP:
                    damageStartEnemyBuildup(enemy, hitScratch->hitKey, 0);
                    state = work->state;
                    if (state == ODD_STRANGER_STATE_DOWN || state == ODD_STRANGER_STATE_STATUS_HOLD) {
                        work->state = ODD_STRANGER_STATE_STATUS_HOLD;
                    } else if (work->state == ODD_STRANGER_STATE_RISE_FRONT && work->stateTimer < ODD_STRANGER_HIT_RISE_FRONT_REFALL_FRAMES) {
                        work->state = ODD_STRANGER_STATE_REFALL_FRONT;
                    } else if (work->state == ODD_STRANGER_STATE_RISE_BACK && work->stateTimer < ODD_STRANGER_HIT_RISE_BACK_REFALL_FRAMES) {
                        work->state = ODD_STRANGER_STATE_REFALL_BACK;
                    } else {
                        absHitYaw = hitScratch->hitYaw;
                        if (absHitYaw < 0) {
                            absHitYaw = -absHitYaw;
                        }
                        work->state = (absHitYaw < ODD_STRANGER_HIT_FALL_YAW_LIMIT) ? ODD_STRANGER_STATE_FALL_BACK : ODD_STRANGER_STATE_FALL_FRONT;
                    }
                    break;
                case DAMAGE_PLAYER_REACTION_POISON:
                    state = work->state;
                    if (state == ODD_STRANGER_STATE_PATROL || state == ODD_STRANGER_STATE_DORMANT || state == ODD_STRANGER_STATE_DORMANT_SCRIPTED) {
                        work->state = ODD_STRANGER_STATE_ALERT;
                    }
                    damageTryStartEnemyDamageOverTime(enemy, hitScratch->hitKey, 0);
                    break;
                case DAMAGE_PLAYER_REACTION_STAGGER:
                    enemy->reactionFlags &= ENEMY_REACTION_STAGGER_CLEAR;
                    state                 = work->state;
#if ODD_STRANGER_VARIANT == 1
                    if (state != ODD_STRANGER_STATE_FALL_BACK && state != ODD_STRANGER_STATE_FALL_FRONT && state != ODD_STRANGER_STATE_REFALL_BACK && state != ODD_STRANGER_STATE_REFALL_FRONT && state != ODD_STRANGER_STATE_STATUS_HOLD && state != ODD_STRANGER_STATE_DOWN) {
#else
                    if (state != ODD_STRANGER_STATE_FALL_BACK && state != ODD_STRANGER_STATE_FALL_FRONT && state != ODD_STRANGER_STATE_REFALL_BACK && state != ODD_STRANGER_STATE_REFALL_FRONT && state != ODD_STRANGER_STATE_DOWN && state != ODD_STRANGER_STATE_STATUS_HOLD) {
#endif
                        if (work->state == ODD_STRANGER_STATE_RISE_FRONT && work->stateTimer < ODD_STRANGER_HIT_RISE_FRONT_REFALL_FRAMES) {
                            work->state = ODD_STRANGER_STATE_REFALL_FRONT;
                        } else if (work->state == ODD_STRANGER_STATE_RISE_BACK && work->stateTimer < ODD_STRANGER_HIT_RISE_BACK_REFALL_FRAMES) {
                            work->state = ODD_STRANGER_STATE_REFALL_BACK;
                        } else {
                            absHitYaw = hitScratch->hitYaw;
                            if (absHitYaw < 0) {
                                absHitYaw = -absHitYaw;
                            }
                            work->state = (absHitYaw < ODD_STRANGER_HIT_FALL_YAW_LIMIT) ? ODD_STRANGER_STATE_FALL_BACK : ODD_STRANGER_STATE_FALL_FRONT;
                        }
                    }
                    break;
                case ODD_STRANGER_HIT_REACTION_FRONT_FALL:
                    state = work->state;
#if ODD_STRANGER_VARIANT == 1
                    if (state != ODD_STRANGER_STATE_FALL_BACK && state != ODD_STRANGER_STATE_FALL_FRONT && state != ODD_STRANGER_STATE_REFALL_BACK && state != ODD_STRANGER_STATE_REFALL_FRONT && state != ODD_STRANGER_STATE_STATUS_HOLD && state != ODD_STRANGER_STATE_DOWN) {
#else
                    if (state != ODD_STRANGER_STATE_FALL_BACK && state != ODD_STRANGER_STATE_FALL_FRONT && state != ODD_STRANGER_STATE_REFALL_BACK && state != ODD_STRANGER_STATE_REFALL_FRONT && state != ODD_STRANGER_STATE_DOWN && state != ODD_STRANGER_STATE_STATUS_HOLD) {
#endif
                        absHitYaw = hitScratch->hitYaw;
                        if (absHitYaw < 0) {
                            absHitYaw = -absHitYaw;
                        }
                        if (absHitYaw < ODD_STRANGER_HIT_BEHIND_YAW_MIN) {
                            if (work->state == ODD_STRANGER_STATE_RISE_FRONT && work->stateTimer < ODD_STRANGER_HIT_RISE_FRONT_REFALL_FRAMES) {
                                work->state = ODD_STRANGER_STATE_REFALL_FRONT;
                            } else if (work->state == ODD_STRANGER_STATE_RISE_BACK && work->stateTimer < ODD_STRANGER_HIT_RISE_BACK_REFALL_FRAMES) {
                                work->state = ODD_STRANGER_STATE_REFALL_BACK;
                            } else {
                                absHitYaw = hitScratch->hitYaw;
                                if (absHitYaw < 0) {
                                    absHitYaw = -absHitYaw;
                                }
                                work->state = (absHitYaw < ODD_STRANGER_HIT_FALL_YAW_LIMIT) ? ODD_STRANGER_STATE_FALL_BACK : ODD_STRANGER_STATE_FALL_FRONT;
                            }
                        }
                    }
                    break;
                case ODD_STRANGER_HIT_REACTION_RESTART_FALL:
                    state = work->state;
                    if (state != ODD_STRANGER_STATE_FALL_BACK && state != ODD_STRANGER_STATE_FALL_FRONT && state != ODD_STRANGER_STATE_REFALL_BACK && state != ODD_STRANGER_STATE_REFALL_FRONT && state != ODD_STRANGER_STATE_STATUS_HOLD && state != ODD_STRANGER_STATE_DOWN) {
                        if (work->state == ODD_STRANGER_STATE_RISE_FRONT && work->stateTimer < ODD_STRANGER_HIT_RISE_FRONT_REFALL_FRAMES) {
                            work->state = ODD_STRANGER_STATE_REFALL_FRONT;
                        } else if (work->state == ODD_STRANGER_STATE_RISE_BACK && work->stateTimer < ODD_STRANGER_HIT_RISE_BACK_REFALL_FRAMES) {
                            work->state = ODD_STRANGER_STATE_REFALL_BACK;
                        } else {
                            absHitYaw = hitScratch->hitYaw;
                            if (absHitYaw < 0) {
                                absHitYaw = -absHitYaw;
                            }
                            work->state = (absHitYaw < ODD_STRANGER_HIT_FALL_YAW_LIMIT) ? ODD_STRANGER_STATE_FALL_BACK : ODD_STRANGER_STATE_FALL_FRONT;
                        }
                    }
                    break;
            }
            work->recentDamageTimer = ODD_STRANGER_HIT_RECENT_FRAMES;
        } else if (work->recentDamageTimer <= 0) {
            work->recentDamage = 0;
        } else {
            work->recentDamageTimer = (u16)work->recentDamageTimer - 1;
        }
        // Status damage advances even on a frame without a new hit.
        if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
            hitScratch->damage = damageTickEnemyDamageOverTime(enemy);
            if (damageIsEnemyDamageOverTimeExpired(enemy) != 0) {
                enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
            }
            if (hitScratch->damage != 0) {
                enemy->hp -= hitScratch->damage;
                worldTargetAddReadoutAmount(&enemy->node, hitScratch->damage, 0);
#if ODD_STRANGER_VARIANT == 1
                if (work->state == ODD_STRANGER_STATE_CHASE || work->state == ODD_STRANGER_STATE_STALK || work->state == ODD_STRANGER_STATE_GRAB || work->state == ODD_STRANGER_STATE_WATCH) {
                    work->state = ODD_STRANGER_STATE_FLINCH;
                } else if (work->state == ODD_STRANGER_STATE_STATUS_HOLD) {
                    work->prevState = -1;
                } else {
                    if (work->state == ODD_STRANGER_STATE_FALL_BACK || work->state == ODD_STRANGER_STATE_FALL_FRONT || work->state == ODD_STRANGER_STATE_RISE_BACK || work->state == ODD_STRANGER_STATE_RISE_FRONT || work->state == ODD_STRANGER_STATE_DOWN) {
                        if (work->animId == ODD_STRANGER_ANIM_DOWN_BACK || work->animId == ODD_STRANGER_ANIM_STATUS_BACK || work->animId == ODD_STRANGER_ANIM_RISE_BACK || work->animId == ODD_STRANGER_HIT_ANIM_FALL_BACK) {
                            work->blendActive = 1;
                            work->blendAnimId = ODD_STRANGER_ANIM_DOWN_BACK;
                        } else {
                            work->blendActive = 1;
                            work->blendAnimId = ODD_STRANGER_ANIM_REFALL_FRONT;
                        }
                    } else {
                        work->blendActive = 1;
                        work->blendAnimId = ODD_STRANGER_ANIM_FLINCH;
                    }
                    work->blendRequest = ODD_STRANGER_ANIM_REQUEST_RESET;
                }
#else
                state         = work->state;
                stateOrHitKey = (u16)work->state;
                if (state == ODD_STRANGER_STATE_CHASE || state == ODD_STRANGER_STATE_STALK || state == ODD_STRANGER_STATE_GRAB || state == ODD_STRANGER_STATE_WATCH) {
                    work->state = ODD_STRANGER_STATE_FLINCH;
                } else if ((u16)(stateOrHitKey - ODD_STRANGER_STATE_FALL_BACK) < 2 || state == ODD_STRANGER_STATE_RISE_BACK || state == ODD_STRANGER_STATE_RISE_FRONT || state == ODD_STRANGER_STATE_STATUS_HOLD || state == ODD_STRANGER_STATE_DOWN) {
                    if (work->animId == ODD_STRANGER_ANIM_DOWN_BACK || work->animId == ODD_STRANGER_ANIM_STATUS_BACK || work->animId == ODD_STRANGER_ANIM_RISE_BACK || work->animId == ODD_STRANGER_HIT_ANIM_FALL_BACK) {
                        work->blendActive = 1;
                        work->blendAnimId = ODD_STRANGER_ANIM_DOWN_BACK;
                    } else {
                        work->blendActive = 1;
                        work->blendAnimId = ODD_STRANGER_ANIM_REFALL_FRONT;
                    }
                    work->blendRequest = ODD_STRANGER_ANIM_REQUEST_RESET;
                } else {
                    work->blendActive  = 1;
                    work->blendAnimId  = ODD_STRANGER_ANIM_FLINCH;
                    work->blendRequest = ODD_STRANGER_ANIM_REQUEST_RESET;
                }
#endif
            }
        }
        if (enemy->hp <= 0) {
#if ODD_STRANGER_VARIANT == 1
            if (hitScratch->hitKey != 0) {
                if ((damageGetPlayerAttackReaction(hitScratch->hitKey) & ODD_STRANGER_HIT_REACTION_MASK) == ODD_STRANGER_HIT_REACTION_FALL || (damageGetPlayerAttackReaction(hitScratch->hitKey) & ODD_STRANGER_HIT_REACTION_MASK) == DAMAGE_PLAYER_REACTION_EXPLOSION) {
                    if ((u16)(work->animId - ODD_STRANGER_ANIM_WALK) < 2) {
#else
            stateOrHitKey = hitScratch->hitKey;
            if (stateOrHitKey != 0) {
                if ((damageGetPlayerAttackReaction(stateOrHitKey) & ODD_STRANGER_HIT_REACTION_MASK) == ODD_STRANGER_HIT_REACTION_FALL || (damageGetPlayerAttackReaction(hitScratch->hitKey) & ODD_STRANGER_HIT_REACTION_MASK) == DAMAGE_PLAYER_REACTION_EXPLOSION) {
                    deathAnim = work->animId;
                    if (deathAnim == ODD_STRANGER_ANIM_WALK || deathAnim == ODD_STRANGER_ANIM_RUN) {
#endif
                        work->state = ODD_STRANGER_STATE_DEATH_BURST_WALK;
                    } else {
                        work->state = ODD_STRANGER_STATE_DEATH_BURST;
                    }
                } else {
                    state = work->state;
                    if (state != ODD_STRANGER_STATE_FALL_BACK && state != ODD_STRANGER_STATE_FALL_FRONT
#if ODD_STRANGER_VARIANT == 1
                        && state != ODD_STRANGER_STATE_STATUS_HOLD
#endif
                        && state != ODD_STRANGER_STATE_DOWN) {
                        if (work->state == ODD_STRANGER_STATE_RISE_FRONT && work->stateTimer < ODD_STRANGER_HIT_RISE_FRONT_REFALL_FRAMES) {
                            work->state = ODD_STRANGER_STATE_REFALL_FRONT;
                        } else if (work->state == ODD_STRANGER_STATE_RISE_BACK && work->stateTimer < ODD_STRANGER_HIT_RISE_BACK_REFALL_FRAMES) {
                            work->state = ODD_STRANGER_STATE_REFALL_BACK;
                        } else {
                            absHitYaw = hitScratch->hitYaw;
                            if (absHitYaw < 0) {
                                absHitYaw = -absHitYaw;
                            }
                            work->state = (absHitYaw < ODD_STRANGER_HIT_FALL_YAW_LIMIT) ? ODD_STRANGER_STATE_FALL_BACK : ODD_STRANGER_STATE_FALL_FRONT;
                        }
                    }
                }
            } else {
                if (work->playerHeld == 1) {
                    taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, ODD_STRANGER_DEATH_RELEASE_ARG, 0);
                    work->playerHeld = 0;
                }
                state = work->state;
#if ODD_STRANGER_VARIANT == 1
                if (state != ODD_STRANGER_STATE_FALL_BACK && state != ODD_STRANGER_STATE_FALL_FRONT && state != ODD_STRANGER_STATE_DEATH_BURN && state != ODD_STRANGER_STATE_DEATH_BURST && state != ODD_STRANGER_STATE_HIDDEN && state != ODD_STRANGER_STATE_STATUS_HOLD && state != ODD_STRANGER_STATE_REFALL_BACK && state != ODD_STRANGER_STATE_REFALL_FRONT && state != ODD_STRANGER_STATE_DOWN) {
#else
                if ((u16)(state - ODD_STRANGER_STATE_FALL_BACK) >= 3 && state != ODD_STRANGER_STATE_DEATH_BURST && state != ODD_STRANGER_STATE_DEATH_BURST_WALK && state != ODD_STRANGER_STATE_HIDDEN && state != ODD_STRANGER_STATE_REFALL_BACK && state != ODD_STRANGER_STATE_REFALL_FRONT && state != ODD_STRANGER_STATE_DOWN) {
#endif
                    if (work->state == ODD_STRANGER_STATE_RISE_FRONT && work->stateTimer < ODD_STRANGER_HIT_RISE_FRONT_REFALL_FRAMES) {
                        work->state = ODD_STRANGER_STATE_REFALL_FRONT;
                    } else if (work->state == ODD_STRANGER_STATE_RISE_BACK && work->stateTimer < ODD_STRANGER_HIT_RISE_BACK_REFALL_FRAMES) {
                        work->state = ODD_STRANGER_STATE_REFALL_BACK;
                    } else {
                        work->state = ODD_STRANGER_STATE_FALL_FRONT;
                    }
                }
            }
        }
        SCRATCH_STACK_RELEASE_BLOCK(ActorHitScratch);
    }
}
