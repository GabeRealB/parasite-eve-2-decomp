/* Part of the Odd Stranger library; see odd_stranger.h. */

/// Per-frame hit intake while HP > 0: looks up a hit record on the contact tables, spawns the hit effect, knocks the root back along the hit direction, computes damage (x4 on a critical roll, x2 from behind), plays the hit/death sound and picks the reaction state from the hit kind. Also ticks the damage-over-time flag and selects the death state (0x1D/0x21/0x13/0x14) once HP runs out.
void oddStrangerTakeHit(Task* arg0)
{
    PlayerStatus*    config = &gPlayerStatus;
    OddStrangerWork* work;
    Enemy*           enemy;
    ActorHitScratch* head;
    ActorHitScratch* s;
    GfxCoord*        coord;
    Task*            player;
    SVECTOR*         dir;
    s16              z;
    s32              yaw;
    s32              dx;
    s32              dy;
    s32              dz;
    s32              deathSound;
    s32              deathPan;
    s32              hitSound;
    s32              hitPan;
    s32              mag;
#if ODD_STRANGER_VARIANT == 2
    s32 value;
#endif
    s16 state;
#if ODD_STRANGER_VARIANT == 2
    s16 animState;
#endif
    s16 effect;
    u32 damage;

    enemy = arg0->spawnArg2.pointer;
    work  = arg0->work;
    if (enemy->hp > 0) {
        head      = SCRATCH_STACK_CURSOR(ActorHitScratch);
        s         = (SCRATCH_STACK_CURSOR(ActorHitScratch) = head - 1);
        s->hitKey = actorFindHit(&head[-1].hitPos, work->hitContacts);
#if ODD_STRANGER_VARIANT == 2
        if (s->hitKey == 0) {
            s->hitKey = actorFindHit(&s->hitPos, work->gridContacts);
        }
#endif
        if (s->hitKey != 0) {
            if (s->hitKey & 0x8000) {
                player       = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                s->hitPos.vx = player->extra.tmd->coords->workm.t[0];
                s->hitPos.vy = player->extra.tmd->coords->workm.t[1];
                s->hitPos.vz = player->extra.tmd->coords->workm.t[2];
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
            arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(arg0->extra.tmd->coords);
            s->hitOffset.vx = arg0->extra.tmd->coords->workm.t[0];
            s->hitOffset.vy = arg0->extra.tmd->coords->workm.t[1];
            s->hitOffset.vz = arg0->extra.tmd->coords->workm.t[2];
            s->hitOffset.vx = s->hitPos.vx - arg0->extra.tmd->coords->workm.t[0];
            s->hitOffset.vy = s->hitPos.vy - arg0->extra.tmd->coords->workm.t[1];
            z               = s->hitPos.vz - arg0->extra.tmd->coords->workm.t[2];
            s->hitOffset.vz = z;
            yaw             = ratan2(s->hitOffset.vx, z);
            coord           = arg0->extra.tmd->coords;
            s->hitYaw       = yaw - ratan2(-coord->workm.m[2][0], coord->workm.m[2][2]);
            s->hitYaw       = actorNormalizeYaw(s->hitYaw);
            oddStrangerSpawnHitEffect(arg0, s->hitYaw, s->hitKey);
            work->lookYaw       = 0;
            work->lookYawTarget = 0;
            s->criticalEffect   = -1;
            state               = work->state;
            if (state != ODD_STRANGER_STATE_FALL_BACK && state != ODD_STRANGER_STATE_FALL_FRONT && state != ODD_STRANGER_STATE_DOWN && state != ODD_STRANGER_STATE_REFALL_BACK && state != ODD_STRANGER_STATE_REFALL_FRONT && state != ODD_STRANGER_STATE_RISE_BACK && state != ODD_STRANGER_STATE_RISE_FRONT && state != ODD_STRANGER_STATE_STATUS_HOLD) {
                s->towardHit = arg0->extra.tmd->coords->coord;
                gfxRotMatrixY(&s->towardHit, s->hitYaw, 0);
                dir = &s->hitOffset;
                gfxReadMatrixZAxis(&s->towardHit, dir);
                VectorNormalSS(dir, dir);
                if (work->recentDamageTimer > 0) {
                    gte_lddp(-0x19);
                    gte_ldsv(dir);
                    gte_gpf12();
                    gte_stsv(dir);
                } else {
                    gte_lddp(-0x64);
                    gte_ldsv(dir);
                    gte_gpf12();
                    gte_stsv(dir);
                }
                arg0->extra.tmd->coords->coord.t[0]  += s->hitOffset.vx;
                arg0->extra.tmd->coords->coord.t[1]  += s->hitOffset.vy;
                arg0->extra.tmd->coords->coord.t[2]  += s->hitOffset.vz;
                arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            }
            dx                = config->coordMtx->t[0] - arg0->extra.tmd->coords->coord.t[0];
            s->toPlayer.vx    = dx;
            dy                = config->coordMtx->t[1] - arg0->extra.tmd->coords->coord.t[1];
            s->toPlayer.vy    = dy;
            dz                = config->coordMtx->t[2] - arg0->extra.tmd->coords->coord.t[2];
            s->toPlayer.vz    = dz;
            s->playerDistance = SquareRoot0(dx * dx + dy * dy + dz * dz);
            s->damage         = Gp_ComputeDamage(s->hitKey, s->playerDistance, 0, 0);
            if (Gp_RollEnemyChance(enemy, s->hitKey, 0) != 0) {
                s->critical       = 1;
                s->criticalEffect = 0;
                s->damage        *= 4;
            } else {
                s->critical = 0;
            }
            mag = s->hitYaw;
            if (mag < 0) {
                mag = -mag;
            }
            if (mag >= 0x501) {
                state = work->state;
                if (state != ODD_STRANGER_STATE_FALL_BACK) {
                    if (state != ODD_STRANGER_STATE_FALL_FRONT && state != ODD_STRANGER_STATE_DOWN && state != ODD_STRANGER_STATE_REFALL_BACK && state != ODD_STRANGER_STATE_REFALL_FRONT && state != ODD_STRANGER_STATE_RISE_BACK && state != ODD_STRANGER_STATE_RISE_FRONT && state != ODD_STRANGER_STATE_STATUS_HOLD) {
                        damage    = s->damage * 2;
                        s->damage = damage;
                        if (damage != 0) {
                            s->criticalEffect = 4;
                        }
                    }
                }
            }
            func_800E2C78(enemy, s->hitKey, s->damage, 0);
            enemy->hp -= s->damage;
            func_800DA6E8(&enemy->node, s->damage, 0);
            work->recentDamage += s->damage;
            effect              = s->criticalEffect;
            if (effect != -1) {
                Gp_SpawnEff(EFFECT_CRITICAL_HIT, &arg0->extra.tmd->coords[2], (s32)(effect), NULL);
            }
            if (work->state == ODD_STRANGER_STATE_DORMANT_SCRIPTED) {
                sndEvtRequestScriptStop(SOUND_ACROPOLIS_PATIO_STRANGER_DORMANT, SOUND_SCRIPT_STOP_KEEP_RELEASE);
            }
            if ((work->state == ODD_STRANGER_STATE_GRAB_PULL || work->state == ODD_STRANGER_STATE_GRAB_STRIKE || work->state == ODD_STRANGER_STATE_GRAB_RELEASE) && config->hp > 0 && work->playerHeld == 1) {
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
            }
            if (enemy->hp <= 0) {
                deathSound = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x400A0008;
                deathPan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                sndEvtRequestScriptStart(deathSound, deathPan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
            } else {
                hitSound = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x400A0007;
                hitPan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                sndEvtRequestScriptStart(hitSound, hitPan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
            }
            work->hitCooldown = Gp_GetIdParam2(s->hitKey);
            switch (Gp_GetIdParam0(s->hitKey) & 0xFFFF) {
                case 4:
                    state = work->state;
                    if (state != ODD_STRANGER_STATE_FALL_BACK && state != ODD_STRANGER_STATE_FALL_FRONT && state != ODD_STRANGER_STATE_REFALL_BACK && state != ODD_STRANGER_STATE_REFALL_FRONT
#if ODD_STRANGER_VARIANT == 1
                        && state != ODD_STRANGER_STATE_STATUS_HOLD
#endif
                        && state != ODD_STRANGER_STATE_DOWN) {
                        if (work->state == ODD_STRANGER_STATE_RISE_FRONT && work->stateTimer < 0x21) {
                            work->state = ODD_STRANGER_STATE_REFALL_FRONT;
                        } else if (work->state == ODD_STRANGER_STATE_RISE_BACK && work->stateTimer < 0xC) {
                            work->state = ODD_STRANGER_STATE_REFALL_BACK;
                        } else {
                            mag = s->hitYaw;
                            if (mag < 0) {
                                mag = -mag;
                            }
                            work->state = (mag < 0x400) ? ODD_STRANGER_STATE_FALL_BACK : ODD_STRANGER_STATE_FALL_FRONT;
                        }
                    }
                    break;
                case 0:
                case 5:
                case 6:
                case 7:
#if ODD_STRANGER_VARIANT == 2
                    if (work->state == ODD_STRANGER_STATE_PATROL || work->state == ODD_STRANGER_STATE_DORMANT || work->state == ODD_STRANGER_STATE_DORMANT_SCRIPTED) {
                        work->state = ODD_STRANGER_STATE_ALERT;
                    }
#endif
                    state = work->state;
                    if (state == ODD_STRANGER_STATE_FALL_BACK || state == ODD_STRANGER_STATE_FALL_FRONT || state == ODD_STRANGER_STATE_RISE_BACK || state == ODD_STRANGER_STATE_RISE_FRONT || state == ODD_STRANGER_STATE_STATUS_HOLD || state == ODD_STRANGER_STATE_DOWN) {
                        if (work->animId == 0xB || work->animId == 0x17 || work->animId == 8 || work->animId == 0xA) {
                            work->blendActive = 1;
                            work->blendAnimId = 0xB;
                        } else {
                            work->blendActive = 1;
                            work->blendAnimId = 0x19;
                        }
                        work->blendRequest = ODD_STRANGER_ANIM_REQUEST_RESET;
                    } else if (work->recentDamage >= ODD_STRANGER_STAGGER_DAMAGE || s->critical == 1) {
                        if (work->state == ODD_STRANGER_STATE_RISE_FRONT && work->stateTimer < 0x21) {
                            work->state = ODD_STRANGER_STATE_REFALL_FRONT;
                        } else if (work->state == ODD_STRANGER_STATE_RISE_BACK && work->stateTimer < 0xC) {
                            work->state = ODD_STRANGER_STATE_REFALL_BACK;
                        } else {
                            mag = s->hitYaw;
                            if (mag < 0) {
                                mag = -mag;
                            }
                            work->state = (mag < 0x400) ? ODD_STRANGER_STATE_FALL_BACK : ODD_STRANGER_STATE_FALL_FRONT;
                        }
                    } else {
                        work->blendAnimId  = 0xD;
                        work->blendActive  = 1;
                        work->blendRequest = ODD_STRANGER_ANIM_REQUEST_RESET;
                    }
                    break;
                case 2:
                    Gp_SetObjFlag2(enemy, s->hitKey, 0);
                    state = work->state;
                    if (state == ODD_STRANGER_STATE_DOWN || state == ODD_STRANGER_STATE_STATUS_HOLD) {
                        work->state = ODD_STRANGER_STATE_STATUS_HOLD;
                    } else if (work->state == ODD_STRANGER_STATE_RISE_FRONT && work->stateTimer < 0x21) {
                        work->state = ODD_STRANGER_STATE_REFALL_FRONT;
                    } else if (work->state == ODD_STRANGER_STATE_RISE_BACK && work->stateTimer < 0xC) {
                        work->state = ODD_STRANGER_STATE_REFALL_BACK;
                    } else {
                        mag = s->hitYaw;
                        if (mag < 0) {
                            mag = -mag;
                        }
                        work->state = (mag < 0x400) ? ODD_STRANGER_STATE_FALL_BACK : ODD_STRANGER_STATE_FALL_FRONT;
                    }
                    break;
                case 3:
                    state = work->state;
                    if (state == ODD_STRANGER_STATE_PATROL || state == ODD_STRANGER_STATE_DORMANT || state == ODD_STRANGER_STATE_DORMANT_SCRIPTED) {
                        work->state = ODD_STRANGER_STATE_ALERT;
                    }
                    Gp_SetObjFlag4(enemy, s->hitKey, 0);
                    break;
                case 1:
                    enemy->reactionFlags &= ENEMY_REACTION_STAGGER_CLEAR;
                    state                 = work->state;
#if ODD_STRANGER_VARIANT == 1
                    if (state != ODD_STRANGER_STATE_FALL_BACK && state != ODD_STRANGER_STATE_FALL_FRONT && state != ODD_STRANGER_STATE_REFALL_BACK && state != ODD_STRANGER_STATE_REFALL_FRONT && state != ODD_STRANGER_STATE_STATUS_HOLD && state != ODD_STRANGER_STATE_DOWN) {
#else
                    if (state != ODD_STRANGER_STATE_FALL_BACK && state != ODD_STRANGER_STATE_FALL_FRONT && state != ODD_STRANGER_STATE_REFALL_BACK && state != ODD_STRANGER_STATE_REFALL_FRONT && state != ODD_STRANGER_STATE_DOWN && state != ODD_STRANGER_STATE_STATUS_HOLD) {
#endif
                        if (work->state == ODD_STRANGER_STATE_RISE_FRONT && work->stateTimer < 0x21) {
                            work->state = ODD_STRANGER_STATE_REFALL_FRONT;
                        } else if (work->state == ODD_STRANGER_STATE_RISE_BACK && work->stateTimer < 0xC) {
                            work->state = ODD_STRANGER_STATE_REFALL_BACK;
                        } else {
                            mag = s->hitYaw;
                            if (mag < 0) {
                                mag = -mag;
                            }
                            work->state = (mag < 0x400) ? ODD_STRANGER_STATE_FALL_BACK : ODD_STRANGER_STATE_FALL_FRONT;
                        }
                    }
                    break;
                case 8:
                    state = work->state;
#if ODD_STRANGER_VARIANT == 1
                    if (state != ODD_STRANGER_STATE_FALL_BACK && state != ODD_STRANGER_STATE_FALL_FRONT && state != ODD_STRANGER_STATE_REFALL_BACK && state != ODD_STRANGER_STATE_REFALL_FRONT && state != ODD_STRANGER_STATE_STATUS_HOLD && state != ODD_STRANGER_STATE_DOWN) {
#else
                    if (state != ODD_STRANGER_STATE_FALL_BACK && state != ODD_STRANGER_STATE_FALL_FRONT && state != ODD_STRANGER_STATE_REFALL_BACK && state != ODD_STRANGER_STATE_REFALL_FRONT && state != ODD_STRANGER_STATE_DOWN && state != ODD_STRANGER_STATE_STATUS_HOLD) {
#endif
                        mag = s->hitYaw;
                        if (mag < 0) {
                            mag = -mag;
                        }
                        if (mag < 0x501) {
                            if (work->state == ODD_STRANGER_STATE_RISE_FRONT && work->stateTimer < 0x21) {
                                work->state = ODD_STRANGER_STATE_REFALL_FRONT;
                            } else if (work->state == ODD_STRANGER_STATE_RISE_BACK && work->stateTimer < 0xC) {
                                work->state = ODD_STRANGER_STATE_REFALL_BACK;
                            } else {
                                mag = s->hitYaw;
                                if (mag < 0) {
                                    mag = -mag;
                                }
                                work->state = (mag < 0x400) ? ODD_STRANGER_STATE_FALL_BACK : ODD_STRANGER_STATE_FALL_FRONT;
                            }
                        }
                    }
                    break;
                case 9:
                    state = work->state;
                    if (state != ODD_STRANGER_STATE_FALL_BACK && state != ODD_STRANGER_STATE_FALL_FRONT && state != ODD_STRANGER_STATE_REFALL_BACK && state != ODD_STRANGER_STATE_REFALL_FRONT && state != ODD_STRANGER_STATE_STATUS_HOLD && state != ODD_STRANGER_STATE_DOWN) {
                        if (work->state == ODD_STRANGER_STATE_RISE_FRONT && work->stateTimer < 0x21) {
                            work->state = ODD_STRANGER_STATE_REFALL_FRONT;
                        } else if (work->state == ODD_STRANGER_STATE_RISE_BACK && work->stateTimer < 0xC) {
                            work->state = ODD_STRANGER_STATE_REFALL_BACK;
                        } else {
                            mag = s->hitYaw;
                            if (mag < 0) {
                                mag = -mag;
                            }
                            work->state = (mag < 0x400) ? ODD_STRANGER_STATE_FALL_BACK : ODD_STRANGER_STATE_FALL_FRONT;
                        }
                    }
                    break;
            }
            work->recentDamageTimer = 5;
        } else if (work->recentDamageTimer <= 0) {
            work->recentDamage = 0;
        } else {
            work->recentDamageTimer = (u16)work->recentDamageTimer - 1;
        }
        if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
            s->damage = Gp_TickObjFlag4(enemy);
            if (Gp_ObjFlag4Expired(enemy) != 0) {
                enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
            }
            if (s->damage != 0) {
                enemy->hp -= s->damage;
                func_800DA6E8(&enemy->node, s->damage, 0);
#if ODD_STRANGER_VARIANT == 1
                if (work->state == ODD_STRANGER_STATE_CHASE || work->state == ODD_STRANGER_STATE_STALK || work->state == ODD_STRANGER_STATE_GRAB || work->state == ODD_STRANGER_STATE_WATCH) {
                    work->state = ODD_STRANGER_STATE_FLINCH;
                } else if (work->state == ODD_STRANGER_STATE_STATUS_HOLD) {
                    work->prevState = -1;
                } else {
                    if (work->state == ODD_STRANGER_STATE_FALL_BACK || work->state == ODD_STRANGER_STATE_FALL_FRONT || work->state == ODD_STRANGER_STATE_RISE_BACK || work->state == ODD_STRANGER_STATE_RISE_FRONT || work->state == ODD_STRANGER_STATE_DOWN) {
                        if (work->animId == 0xB || work->animId == 0x17 || work->animId == 8 || work->animId == 0xA) {
                            work->blendActive = 1;
                            work->blendAnimId = 0xB;
                        } else {
                            work->blendActive = 1;
                            work->blendAnimId = 0x19;
                        }
                    } else {
                        work->blendActive = 1;
                        work->blendAnimId = 0xD;
                    }
                    work->blendRequest = ODD_STRANGER_ANIM_REQUEST_RESET;
                }
#else
                state = work->state;
                value = (u16)work->state;
                if (state == ODD_STRANGER_STATE_CHASE || state == ODD_STRANGER_STATE_STALK || state == ODD_STRANGER_STATE_GRAB || state == ODD_STRANGER_STATE_WATCH) {
                    work->state = ODD_STRANGER_STATE_FLINCH;
                } else if ((u16)(value - ODD_STRANGER_STATE_FALL_BACK) < 2 || state == ODD_STRANGER_STATE_RISE_BACK || state == ODD_STRANGER_STATE_RISE_FRONT || state == ODD_STRANGER_STATE_STATUS_HOLD || state == ODD_STRANGER_STATE_DOWN) {
                    if (work->animId == 0xB || work->animId == 0x17 || work->animId == 8 || work->animId == 0xA) {
                        work->blendActive = 1;
                        work->blendAnimId = 0xB;
                    } else {
                        work->blendActive = 1;
                        work->blendAnimId = 0x19;
                    }
                    work->blendRequest = ODD_STRANGER_ANIM_REQUEST_RESET;
                } else {
                    work->blendActive  = 1;
                    work->blendAnimId  = 0xD;
                    work->blendRequest = ODD_STRANGER_ANIM_REQUEST_RESET;
                }
#endif
            }
        }
        if (enemy->hp <= 0) {
#if ODD_STRANGER_VARIANT == 1
            if (s->hitKey != 0) {
                if ((Gp_GetIdParam0(s->hitKey) & 0xFFFF) == 4 || (Gp_GetIdParam0(s->hitKey) & 0xFFFF) == 6) {
                    if ((u16)(work->animId - 2) < 2) {
#else
            value = s->hitKey;
            if (value != 0) {
                if ((Gp_GetIdParam0(value) & 0xFFFF) == 4 || (Gp_GetIdParam0(s->hitKey) & 0xFFFF) == 6) {
                    animState = work->animId;
                    if (animState == 2 || animState == 3) {
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
                        if (work->state == ODD_STRANGER_STATE_RISE_FRONT && work->stateTimer < 0x21) {
                            work->state = ODD_STRANGER_STATE_REFALL_FRONT;
                        } else if (work->state == ODD_STRANGER_STATE_RISE_BACK && work->stateTimer < 0xC) {
                            work->state = ODD_STRANGER_STATE_REFALL_BACK;
                        } else {
                            mag = s->hitYaw;
                            if (mag < 0) {
                                mag = -mag;
                            }
                            work->state = (mag < 0x400) ? ODD_STRANGER_STATE_FALL_BACK : ODD_STRANGER_STATE_FALL_FRONT;
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
                    if (work->state == ODD_STRANGER_STATE_RISE_FRONT && work->stateTimer < 0x21) {
                        work->state = ODD_STRANGER_STATE_REFALL_FRONT;
                    } else if (work->state == ODD_STRANGER_STATE_RISE_BACK && work->stateTimer < 0xC) {
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
