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
    s16 timer;
    u32 damage;

    enemy = arg0->spawnArg2.pointer;
    work  = arg0->work;
    if (enemy->hp > 0) {
        head  = SCRATCH_STACK_CURSOR(ActorHitScratch);
        s     = (SCRATCH_STACK_CURSOR(ActorHitScratch) = head - 1);
        s->id = actorFindHit(&head[-1].hitPos, work->field_8F0);
#if ODD_STRANGER_VARIANT == 2
        if (s->id == 0) {
            s->id = actorFindHit(&s->hitPos, work->field_A30);
        }
#endif
        if (s->id != 0) {
            if (s->id & 0x8000) {
                player       = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                s->hitPos.vx = player->extra.tmd->coords->workm.t[0];
                s->hitPos.vy = player->extra.tmd->coords->workm.t[1];
                s->hitPos.vz = player->extra.tmd->coords->workm.t[2];
            }
            if (work->field_C28 == 1) {
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                work->field_C28 = 0;
                if (work->field_0 == 0xB || work->field_0 == 0xC || work->field_0 == 0xD || work->field_0 == 0xE) {
                    work->field_0 = 0x13;
                }
            }
            work->field_C24                       = 0;
            work->field_C26                       = 0;
            arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(arg0->extra.tmd->coords);
            s->dir.vx = arg0->extra.tmd->coords->workm.t[0];
            s->dir.vy = arg0->extra.tmd->coords->workm.t[1];
            s->dir.vz = arg0->extra.tmd->coords->workm.t[2];
            s->dir.vx = s->hitPos.vx - arg0->extra.tmd->coords->workm.t[0];
            s->dir.vy = s->hitPos.vy - arg0->extra.tmd->coords->workm.t[1];
            z         = s->hitPos.vz - arg0->extra.tmd->coords->workm.t[2];
            s->dir.vz = z;
            yaw       = ratan2(s->dir.vx, z);
            coord     = arg0->extra.tmd->coords;
            s->yaw    = yaw - ratan2(-coord->workm.m[2][0], coord->workm.m[2][2]);
            s->yaw    = actorNormalizeYaw(s->yaw);
            oddStrangerSpawnHitEffect(arg0, s->yaw, s->id);
            work->field_8B0 = 0;
            work->field_8AE = 0;
            s->effect       = -1;
            state           = work->field_0;
            if (state != 0x13 && state != 0x14 && state != 0x11 && state != 0x1F && state != 0x20 && state != 0xF && state != 0x10 && state != 4) {
                s->m = arg0->extra.tmd->coords->coord;
                gfxRotMatrixY(&s->m, s->yaw, 0);
                dir = &s->dir;
                gfxReadMatrixZAxis(&s->m, dir);
                VectorNormalSS(dir, dir);
                if (work->field_BEC > 0) {
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
                arg0->extra.tmd->coords->coord.t[0]  += s->dir.vx;
                arg0->extra.tmd->coords->coord.t[1]  += s->dir.vy;
                arg0->extra.tmd->coords->coord.t[2]  += s->dir.vz;
                arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            }
            dx        = config->coordMtx->t[0] - arg0->extra.tmd->coords->coord.t[0];
            s->dx     = dx;
            dy        = config->coordMtx->t[1] - arg0->extra.tmd->coords->coord.t[1];
            s->dy     = dy;
            dz        = config->coordMtx->t[2] - arg0->extra.tmd->coords->coord.t[2];
            s->dz     = dz;
            s->dist   = SquareRoot0(dx * dx + dy * dy + dz * dz);
            s->damage = Gp_ComputeDamage(s->id, s->dist, 0, 0);
            if (Gp_RollEnemyChance(enemy, s->id, 0) != 0) {
                s->crit    = 1;
                s->effect  = 0;
                s->damage *= 4;
            } else {
                s->crit = 0;
            }
            mag = s->yaw;
            if (mag < 0) {
                mag = -mag;
            }
            if (mag >= 0x501) {
                state = work->field_0;
                if (state != 0x13) {
                    if (state != 0x14 && state != 0x11 && state != 0x1F && state != 0x20 && state != 0xF && state != 0x10 && state != 4) {
                        damage    = s->damage * 2;
                        s->damage = damage;
                        if (damage != 0) {
                            s->effect = 4;
                        }
                    }
                }
            }
            func_800E2C78(enemy, s->id, s->damage, 0);
            enemy->hp -= s->damage;
            func_800DA6E8(&enemy->node, s->damage, 0);
            work->field_BEA += s->damage;
            effect           = s->effect;
            if (effect != -1) {
                Gp_SpawnEff(EFFECT_CRITICAL_HIT, &arg0->extra.tmd->coords[2], (s32)(effect), NULL);
            }
            if (work->field_0 == 0x17) {
                SndEvt_EnqueueType7(SOUND_ACROPOLIS_PATIO_STRANGER_DORMANT, 1);
            }
            if ((work->field_0 == 0xC || work->field_0 == 0xD || work->field_0 == 0xE) && config->hp > 0 && work->field_C28 == 1) {
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
            }
            if (enemy->hp <= 0) {
                deathSound = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x400A0008;
                deathPan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                SndEvt_EnqueueType6(deathSound, deathPan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
            } else {
                hitSound = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x400A0007;
                hitPan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                SndEvt_EnqueueType6(hitSound, hitPan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
            }
            work->field_BE8 = Gp_GetIdParam2(s->id);
            switch (Gp_GetIdParam0(s->id) & 0xFFFF) {
                case 4:
                    state = work->field_0;
                    if (state != 0x13 && state != 0x14 && state != 0x1F && state != 0x20
#if ODD_STRANGER_VARIANT == 1
                        && state != 4
#endif
                        && state != 0x11) {
                        if (work->field_0 == 0x10 && work->field_6 < 0x21) {
                            work->field_0 = 0x20;
                        } else if (work->field_0 == 0xF && work->field_6 < 0xC) {
                            work->field_0 = 0x1F;
                        } else {
                            mag = s->yaw;
                            if (mag < 0) {
                                mag = -mag;
                            }
                            work->field_0 = (mag < 0x400) ? 0x13 : 0x14;
                        }
                    }
                    break;
                case 0:
                case 5:
                case 6:
                case 7:
#if ODD_STRANGER_VARIANT == 2
                    if (work->field_0 == 0x18 || work->field_0 == 0x16 || work->field_0 == 0x17) {
                        work->field_0 = 6;
                    }
#endif
                    state = work->field_0;
                    if (state == 0x13 || state == 0x14 || state == 0xF || state == 0x10 || state == 4 || state == 0x11) {
                        if (work->field_89E == 0xB || work->field_89E == 0x17 || work->field_89E == 8 || work->field_89E == 0xA) {
                            work->field_89A = 1;
                            work->field_8A8 = 0xB;
                        } else {
                            work->field_89A = 1;
                            work->field_8A8 = 0x19;
                        }
                        work->field_8A6 = 2;
                    } else if (work->field_BEA >= ODD_STRANGER_STAGGER_DAMAGE || s->crit == 1) {
                        if (work->field_0 == 0x10 && work->field_6 < 0x21) {
                            work->field_0 = 0x20;
                        } else if (work->field_0 == 0xF && work->field_6 < 0xC) {
                            work->field_0 = 0x1F;
                        } else {
                            mag = s->yaw;
                            if (mag < 0) {
                                mag = -mag;
                            }
                            work->field_0 = (mag < 0x400) ? 0x13 : 0x14;
                        }
                    } else {
                        work->field_8A8 = 0xD;
                        work->field_89A = 1;
                        work->field_8A6 = 2;
                    }
                    break;
                case 2:
                    Gp_SetObjFlag2(enemy, s->id, 0);
                    state = work->field_0;
                    if (state == 0x11 || state == 4) {
                        work->field_0 = 4;
                    } else if (work->field_0 == 0x10 && work->field_6 < 0x21) {
                        work->field_0 = 0x20;
                    } else if (work->field_0 == 0xF && work->field_6 < 0xC) {
                        work->field_0 = 0x1F;
                    } else {
                        mag = s->yaw;
                        if (mag < 0) {
                            mag = -mag;
                        }
                        work->field_0 = (mag < 0x400) ? 0x13 : 0x14;
                    }
                    break;
                case 3:
                    state = work->field_0;
                    if (state == 0x18 || state == 0x16 || state == 0x17) {
                        work->field_0 = 6;
                    }
                    Gp_SetObjFlag4(enemy, s->id, 0);
                    break;
                case 1:
                    enemy->reactionFlags &= ENEMY_REACTION_STAGGER_CLEAR;
                    state                 = work->field_0;
#if ODD_STRANGER_VARIANT == 1
                    if (state != 0x13 && state != 0x14 && state != 0x1F && state != 0x20 && state != 4 && state != 0x11) {
#else
                    if (state != 0x13 && state != 0x14 && state != 0x1F && state != 0x20 && state != 0x11 && state != 4) {
#endif
                        if (work->field_0 == 0x10 && work->field_6 < 0x21) {
                            work->field_0 = 0x20;
                        } else if (work->field_0 == 0xF && work->field_6 < 0xC) {
                            work->field_0 = 0x1F;
                        } else {
                            mag = s->yaw;
                            if (mag < 0) {
                                mag = -mag;
                            }
                            work->field_0 = (mag < 0x400) ? 0x13 : 0x14;
                        }
                    }
                    break;
                case 8:
                    state = work->field_0;
#if ODD_STRANGER_VARIANT == 1
                    if (state != 0x13 && state != 0x14 && state != 0x1F && state != 0x20 && state != 4 && state != 0x11) {
#else
                    if (state != 0x13 && state != 0x14 && state != 0x1F && state != 0x20 && state != 0x11 && state != 4) {
#endif
                        mag = s->yaw;
                        if (mag < 0) {
                            mag = -mag;
                        }
                        if (mag < 0x501) {
                            if (work->field_0 == 0x10 && work->field_6 < 0x21) {
                                work->field_0 = 0x20;
                            } else if (work->field_0 == 0xF && work->field_6 < 0xC) {
                                work->field_0 = 0x1F;
                            } else {
                                mag = s->yaw;
                                if (mag < 0) {
                                    mag = -mag;
                                }
                                work->field_0 = (mag < 0x400) ? 0x13 : 0x14;
                            }
                        }
                    }
                    break;
                case 9:
                    state = work->field_0;
                    if (state != 0x13 && state != 0x14 && state != 0x1F && state != 0x20 && state != 4 && state != 0x11) {
                        if (work->field_0 == 0x10 && work->field_6 < 0x21) {
                            work->field_0 = 0x20;
                        } else if (work->field_0 == 0xF && work->field_6 < 0xC) {
                            work->field_0 = 0x1F;
                        } else {
                            mag = s->yaw;
                            if (mag < 0) {
                                mag = -mag;
                            }
                            work->field_0 = (mag < 0x400) ? 0x13 : 0x14;
                        }
                    }
                    break;
            }
            timer = 5;
        } else if (work->field_BEC > 0) {
            timer = (u16)work->field_BEC - 1;
        } else {
            work->field_BEA = 0;
            goto block_bec;
        }
        work->field_BEC = timer;
    block_bec:
        if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
            s->damage = Gp_TickObjFlag4(enemy);
            if (Gp_ObjFlag4Expired(enemy) != 0) {
                enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
            }
            if (s->damage != 0) {
                enemy->hp -= s->damage;
                func_800DA6E8(&enemy->node, s->damage, 0);
#if ODD_STRANGER_VARIANT == 1
                if (work->field_0 == 7 || work->field_0 == 0x1E || work->field_0 == 0xB || work->field_0 == 0x1B) {
                    work->field_0 = 5;
                } else if (work->field_0 == 4) {
                    work->field_2 = -1;
                } else {
                    if (work->field_0 == 0x13 || work->field_0 == 0x14 || work->field_0 == 0xF || work->field_0 == 0x10 || work->field_0 == 0x11) {
                        if (work->field_89E == 0xB || work->field_89E == 0x17 || work->field_89E == 8 || work->field_89E == 0xA) {
                            work->field_89A = 1;
                            work->field_8A8 = 0xB;
                        } else {
                            work->field_89A = 1;
                            work->field_8A8 = 0x19;
                        }
                    } else {
                        work->field_89A = 1;
                        work->field_8A8 = 0xD;
                    }
                    work->field_8A6 = 2;
                }
#else
                state = work->field_0;
                value = (u16)work->field_0;
                if (state == 7 || state == 0x1E || state == 0xB || state == 0x1B) {
                    work->field_0 = 5;
                } else if ((u16)(value - 0x13) < 2 || state == 0xF || state == 0x10 || state == 4 || state == 0x11) {
                    if (work->field_89E == 0xB || work->field_89E == 0x17 || work->field_89E == 8 || work->field_89E == 0xA) {
                        work->field_89A = 1;
                        work->field_8A8 = 0xB;
                    } else {
                        work->field_89A = 1;
                        work->field_8A8 = 0x19;
                    }
                    work->field_8A6 = 2;
                } else {
                    work->field_89A = 1;
                    work->field_8A8 = 0xD;
                    work->field_8A6 = 2;
                }
#endif
            }
        }
        if (enemy->hp <= 0) {
#if ODD_STRANGER_VARIANT == 1
            if (s->id != 0) {
                if ((Gp_GetIdParam0(s->id) & 0xFFFF) == 4 || (Gp_GetIdParam0(s->id) & 0xFFFF) == 6) {
                    if ((u16)(work->field_89E - 2) < 2) {
#else
            value = s->id;
            if (value != 0) {
                if ((Gp_GetIdParam0(value) & 0xFFFF) == 4 || (Gp_GetIdParam0(s->id) & 0xFFFF) == 6) {
                    animState = work->field_89E;
                    if (animState == 2 || animState == 3) {
#endif
                        work->field_0 = 0x21;
                    } else {
                        work->field_0 = 0x1D;
                    }
                } else {
                    state = work->field_0;
                    if (state != 0x13 && state != 0x14
#if ODD_STRANGER_VARIANT == 1
                        && state != 4
#endif
                        && state != 0x11) {
                        if (work->field_0 == 0x10 && work->field_6 < 0x21) {
                            work->field_0 = 0x20;
                        } else if (work->field_0 == 0xF && work->field_6 < 0xC) {
                            work->field_0 = 0x1F;
                        } else {
                            mag = s->yaw;
                            if (mag < 0) {
                                mag = -mag;
                            }
                            work->field_0 = (mag < 0x400) ? 0x13 : 0x14;
                        }
                    }
                }
            } else {
                if (work->field_C28 == 1) {
                    taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, ODD_STRANGER_DEATH_RELEASE_ARG, 0);
                    work->field_C28 = 0;
                }
                state = work->field_0;
#if ODD_STRANGER_VARIANT == 1
                if (state != 0x13 && state != 0x14 && state != 0x15 && state != 0x1D && state != 0 && state != 4 && state != 0x1F && state != 0x20 && state != 0x11) {
#else
                if ((u16)(state - 0x13) >= 3 && state != 0x1D && state != 0x21 && state != 0 && state != 0x1F && state != 0x20 && state != 0x11) {
#endif
                    if (work->field_0 == 0x10 && work->field_6 < 0x21) {
                        work->field_0 = 0x20;
                    } else if (work->field_0 == 0xF && work->field_6 < 0xC) {
                        work->field_0 = 0x1F;
                    } else {
                        work->field_0 = 0x14;
                    }
                }
            }
        }
        SCRATCH_STACK_RELEASE_BYTES(0x54);
    }
}
