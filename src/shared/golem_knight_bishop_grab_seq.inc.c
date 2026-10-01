/* Part of the Knight/Bishop GOLEM library; see golem_knight_bishop.h. */

/// Runs the actor's hold sequence on the player (the same 0x3F8 / 0x3FF
/// message pair `func_actor_103700_80134F50` uses to take a hold). State 0
/// asks the player for range 0x19 while enemies remain; on success it plants
/// the display object at `field_6A4`, places the player 0x5AA in front of it
/// with message 0x3E9 and queues a cue. States 1 and 2 step the player's
/// animation. State 3 waits out `field_6D6`, then every 0x1E frames decides
/// whether the hold ends: always when `gPlayerStatus.hp` is above the
/// per-difficulty `gGolemKnightBishopGrabHpLimits`, otherwise by an LCG roll whose
/// chance grows with the attempt count `field_6F6`; a raised `field_6F4`
/// ends it early. State 5 either reacts to `field_6F4` or, at frame 0x1A,
/// spawns the spark, sends message 0x400 and clears `gPlayerStatus.hp`; state 7
/// then loads file 9/0x1E and queues cue 0x70010001 once the CD is idle.
void golemKnightBishopGrabSeq(Task* arg0)
{
    GolemKnightBishopWork*        work;
    GfxCoord*                     coord;
    Task*                         player;
    GolemKnightBishopGrabScratch* sc;
    GfxCoord*                     pcoord;
    s32                           flag;
    s32                           snd;
    s32                           chance;
    u32                           random;
    s16                           timer;
    s16                           val;
    s16                           sub;

    work   = arg0->work;
    coord  = arg0->extra.tmd->coords;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    SCRATCH_STACK_RESERVE_BYTES(sizeof(GolemKnightBishopGrabScratch));
    sc     = SCRATCH_STACK_CURSOR(GolemKnightBishopGrabScratch);
    pcoord = player->extra.tmd->coords;
    flag   = 0;
    switch (work->field_6CE) {
        case 0:
            if (((GameActor*)player->work)->mode != GAME_ACTOR_MODE_SCRIPTED && gPlayerStatus.hp > 0) {
                sc->query.field_14 = 0x19;
                if (TASK_MESSAGE_DISPATCH_POINTER(player, 0x3F8, sc, 0) == 0) {
                    work->field_6C0 = 1;
                    work->field_6CE = 1;
                    work->field_6F4 = 0;
                    work->field_6E8 = 0;
                    work->field_718 = 1;
                    sc->in.vx       = 0;
                    sc->in.vy       = work->field_6E6;
                    sc->in.vz       = 0;
                    RotMatrix(&sc->in, &coord->coord);
                    coord->coord.t[0] = work->field_6A4;
                    coord->coord.t[1] = work->field_6A8;
                    coord->coord.t[2] = work->field_6AC;
                    sc->in.vx         = 0;
                    sc->in.vy         = 0;
                    sc->in.vz         = 0x5AA;
                    gte_SetRotMatrix(&coord->coord);
                    gte_ldv0(&sc->in);
                    gte_rtv0();
                    gte_stlvnl(&sc->out);
                    sc->place.pos.vx = coord->coord.t[0] + sc->out.vx;
                    sc->place.pos.vy = coord->coord.t[1] + sc->out.vy;
                    sc->place.pos.vz = coord->coord.t[2] + sc->out.vz;
                    sc->place.rot.vx = 0;
                    sc->place.rot.vy = work->field_6E6;
                    sc->place.rot.vz = 0;
                    TASK_MESSAGE_DISPATCH_POINTER(player, 0x3E9, &sc->place, 0);
                    Gp_SpawnPadLerp(0xA, 0xFF, 0x80);
                    snd = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 6;
                    SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(pcoord), (s8)worldCoordGetOriginAudioDepth(pcoord));
                } else {
                    work->field_6CC = 0;
                    work->field_6CE = 0;
                }
            }
            break;
        case 1:
            sc->anim.source.sets          = gGolemKnightBishopPlayerAnims;
            sc->anim.animationId          = 1;
            sc->anim.blend                = ANIMATION_BLEND_RESET;
            sc->anim.blendFrames          = 0;
            sc->anim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
            TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &sc->anim, 0);
            work->field_6CE = 2;
            work->field_6DC = 0x3C;
            work->field_6DA = 1;
            work->field_6DE = 0x1E;
            work->field_6B8 = gGolemKnightBishopApproachCue | (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
            SndEvt_EnqueueType6(work->field_6B8, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            break;
        case 2:
            if (work->field_6C4 >= 0x29) {
                work->field_6C0               = 2;
                work->field_6CE               = 3;
                work->field_6D6               = 0x1E;
                work->field_6D4               = 0;
                work->field_6F6               = 0;
                sc->anim.source.sets          = gGolemKnightBishopPlayerAnims;
                sc->anim.animationId          = 2;
                sc->anim.blend                = ANIMATION_BLEND_RESET;
                sc->anim.blendFrames          = 0;
                sc->anim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &sc->anim, 0);
                Gp_ArmStateF0(1);
                work->field_70A = 0;
                if (work->field_6C6 == 0) {
                    work->field_49A |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                    work->field_494  = work->field_716 | 0x30000;
                }
            }
            break;
        case 3:
            if (work->field_6D6 > 0) {
                work->field_6D6--;
            } else {
                if ((s16)work->field_6D4 == 2) {
                    Gp_SpawnPadLerp(5, 0xC0, 0x80);
                }
                timer           = work->field_6D4 - 1;
                work->field_6D4 = timer;
                if (timer <= 0) {
                    if (gPlayerStatus.hp > gGolemKnightBishopGrabHpLimits[gSceneCombatState.difficulty]) {
                        if (work->field_6F8 == 0) {
                            work->field_6F8 = 1;
                        } else {
                            chance = work->field_6F6 * (0x32 - (gPlayerStatus.hp * 100) / gPlayerStatus.hpMax) / 2;
                            if (chance > 0) {
                                chance          = (chance * 0xFFF) / 100;
                                gRandomLcgState = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                                if ((s32)((gRandomLcgState >> 16) & 0xFFF) < chance) {
                                    flag = 1;
                                }
                            }
                        }
                    } else {
                        flag = 1;
                    }
                    if (flag != 0) {
                        work->field_6C0               = 3;
                        work->field_6CE               = 5;
                        sc->anim.source.sets          = gGolemKnightBishopPlayerAnims;
                        sc->anim.animationId          = 3;
                        sc->anim.blend                = ANIMATION_BLEND_RESET;
                        sc->anim.blendFrames          = 0;
                        sc->anim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                        TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &sc->anim, 0);
                    } else {
                        work->field_6D4 = GOLEM_KNIGHT_BISHOP_GRAB_HOLD;
                        taskMessageDispatch(player, 0x3F9, Gp_PackPair(gGolemKnightBishopAttacks, 0), 0);
                        work->field_6F6++;
                    }
                }
            }
            if (work->field_6F4 != 0) {
                if (work->field_6F4 == 1) {
                    if (work->field_6D6 > 0 && work->field_6EE == 0) {
                        work->field_6C0 = 0x15;
                        work->field_6CE = 6;
                        work->field_71A = 0;
                        if (work->field_6B8 != 0) {
                            SndEvt_EnqueueType7(work->field_6B8, 1);
                            work->field_6B8 = 0;
                        }
                        work->field_6DA = 7;
                    } else {
                        work->field_6C0 = 0xC;
                        work->field_6CE = 4;
                        work->field_6D4 = 0x69;
                        work->field_6DA = 3;
                        work->field_6DC = 0x4B;
                        work->field_71A = 0;
                        work->field_6DE = 0x1E;
                        work->field_6BC = gGolemKnightBishopPainCue | (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                        SndEvt_EnqueueType6(work->field_6BC, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                    }
                } else {
                    golemKnightBishopPickHitReaction(arg0, work->field_70A);
                    work->field_71A = 0;
                }
                work->field_718               = 2;
                work->field_6F4               = 0;
                sc->anim.source.sets          = gGolemKnightBishopPlayerAnims;
                sc->anim.animationId          = 4;
                sc->anim.blend                = ANIMATION_BLEND_RESET;
                sc->anim.blendFrames          = 0;
                sc->anim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &sc->anim, 0);
            }
            break;
        case 4:
            val = 0;
            if (work->field_6C4 < 0x5F) {
                val = -0xA;
            }
            work->field_6C8 = val;
            timer           = work->field_6D4 - 1;
            work->field_6D4 = timer;
            if (timer <= 0) {
                work->field_6CC = 0;
                work->field_6CE = 0;
            }
            golemKnightBishopHoldCueTimer(arg0);
            break;
        case 5:
            if (work->field_6C4 < 0x1A) {
                if (work->field_6F4 != 0) {
                    work->field_6C0               = 0xC;
                    work->field_6F4               = 0;
                    sc->anim.source.sets          = gGolemKnightBishopPlayerAnims;
                    sc->anim.animationId          = 4;
                    sc->anim.blend                = ANIMATION_BLEND_RESET;
                    sc->anim.blendFrames          = 0;
                    sc->anim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                    TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &sc->anim, 0);
                    work->field_6D4 = 0x69;
                    work->field_6DA = 3;
                    work->field_6DC = 0x4B;
                    work->field_6CE = 4;
                    work->field_6DE = 0x1E;
                    work->field_6BC = gGolemKnightBishopPainCue | (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                    SndEvt_EnqueueType6(work->field_6BC, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                }
            } else if (work->field_6C4 == 0x1A) {
                ((GameActor*)player->work)->state = 0xA;
                work->field_6CE                   = 7;
                work->field_6D4                   = 0;
                gGameSession->deathRestartDelay   = 0x5A;
                gGameSession->deathSoundCountdown = GAME_SESSION_DEATH_SOUND_HOLD;
                sc->in.vy                         = -0x96;
                sc->in.vx                         = 0;
                sc->in.vz                         = 0xC8;
                func_800FDB18(1, &gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords[4], &sc->in, &gGolemKnightBishopGrabEffect);
                Gp_SpawnPadLerp(0xA, 0xFF, 8);
                taskMessageDispatch(player, 0x400, 0, 0);
                gPlayerStatus.hp = 0;
            }
            break;
        case 6:
            work->field_6C8 = -0xA;
            golemKnightBishopHoldCueTimer(arg0);
            if (work->field_718 == 0) {
                work->field_6C8 = 0;
                work->field_6CC = 4;
                work->field_6CE = 0;
            }
            break;
        case 7:
            sub = work->field_6D4;
            switch (sub) {
                case 0:
                    CdCmd_EnqueueLoadFile(9, 0x1E, 3);
                    work->field_6D4 = 1;
                    break;
                case 1:
                    if ((CdCmd_IsIdle() & 0xFFFF) == 1) {
                        coord = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
                        SndEvt_EnqueueType6(0x70010001, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                        work->field_6D4 = 2;
                    }
                    break;
                case 2:
                    break;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(GolemKnightBishopGrabScratch));
}
