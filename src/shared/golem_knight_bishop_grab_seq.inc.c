/* Part of the Knight/Bishop GOLEM library; see golem_knight_bishop.h. */

/// Runs the actor's hold on the player (the same 0x3F8 / 0x3FF message pair
/// `func_actor_103700_80134F50` uses to take a hold). Step 0 asks the player
/// to await 0x19 button presses while they are alive and not scripted; on
/// success it plants the display object at `targetPos` facing `targetYaw`,
/// places the player 0x5AA in front of it with message 0x3E9 and sets
/// `grabStage`. Steps 1 and 2 start the appearance and step the player's
/// animation. Step 3 waits out `auxTimer`, then every
/// `GOLEM_KNIGHT_BISHOP_GRAB_RECHECK` frames either hurts the player, counting
/// `grabDamageTicks`, or goes for the kill: always when `gPlayerStatus.hp` is
/// at or below the per-difficulty `gGolemKnightBishopGrabHpLimits`, otherwise
/// by an LCG roll whose chance grows with `grabDamageTicks` and as the HP
/// falls below half, and never on the check that first sets `grabKillRollArmed`. A raised
/// `grabBreak` ends the hold. The player struggling free backs the golem
/// away: flickering, into the recover sequence, when `auxTimer` is still
/// running and the grab is not `counterattacking` (step 6), and vanishing
/// otherwise (step 4). A weapon hit picks the reaction for
/// `interruptDamage`. Step 5 is the kill: a `grabBreak` before frame 0x1A
/// still frees the player; at 0x1A it spawns the spark, sends message 0x400
/// and clears `gPlayerStatus.hp`, and step 7 then counts in `timer` through
/// loading file 9/0x1E and queuing the death sound once the CD is idle.
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
    switch (work->step) {
        case 0:
            if (((GameActor*)player->work)->mode != GAME_ACTOR_MODE_SCRIPTED && gPlayerStatus.hp > 0) {
                sc->query.pressCount = 0x19;
                if (TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, sc, 0) == 0) {
                    work->anim        = 1;
                    work->step        = 1;
                    work->grabBreak   = 0;
                    work->feintBroken = 0;
                    work->grabStage   = 1;
                    sc->in.vx         = 0;
                    sc->in.vy         = work->targetYaw;
                    sc->in.vz         = 0;
                    RotMatrix(&sc->in, &coord->coord);
                    coord->coord.t[0] = work->targetPos.vx;
                    coord->coord.t[1] = work->targetPos.vy;
                    coord->coord.t[2] = work->targetPos.vz;
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
                    sc->place.rot.vy = work->targetYaw;
                    sc->place.rot.vz = 0;
                    TASK_MESSAGE_DISPATCH_POINTER(player, 0x3E9, &sc->place, 0);
                    Gp_SpawnPadLerp(0xA, 0xFF, 0x80);
                    snd = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 6;
                    SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(pcoord), (s8)worldCoordGetOriginAudioDepth(pcoord));
                } else {
                    work->sequence = GOLEM_KNIGHT_BISHOP_SEQUENCE_IDLE;
                    work->step     = 0;
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
            work->step                   = 2;
            work->translucencyFadeFrames = 0x3C;
            work->fadeState              = GOLEM_KNIGHT_BISHOP_FADE_APPEAR;
            work->colorBlendFadeFrames   = 0x1E;
            work->appearSound            = gGolemKnightBishopApproachCue | (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
            SndEvt_EnqueueType6(work->appearSound, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            break;
        case 2:
            if (work->animFrame >= 0x29) {
                work->anim                    = 2;
                work->step                    = 3;
                work->auxTimer                = 0x1E;
                work->timer                   = 0;
                work->grabDamageTicks         = 0;
                sc->anim.source.sets          = gGolemKnightBishopPlayerAnims;
                sc->anim.animationId          = 2;
                sc->anim.blend                = ANIMATION_BLEND_RESET;
                sc->anim.blendFrames          = 0;
                sc->anim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &sc->anim, 0);
                Gp_ArmStateF0(1);
                work->interruptDamage = 0;
                if (work->hitCooldown == 0) {
                    work->hurtBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                    work->hurtBody.key    = work->actorId | 0x30000;
                }
            }
            break;
        case 3:
            if (work->auxTimer > 0) {
                work->auxTimer--;
            } else {
                if (work->timer == 2) {
                    Gp_SpawnPadLerp(5, 0xC0, 0x80);
                }
                timer       = work->timer - 1;
                work->timer = timer;
                if (timer <= 0) {
                    if (gPlayerStatus.hp > gGolemKnightBishopGrabHpLimits[gSceneCombatState.difficulty]) {
                        if (work->grabKillRollArmed == 0) {
                            work->grabKillRollArmed = 1;
                        } else {
                            chance = work->grabDamageTicks * (0x32 - (gPlayerStatus.hp * 100) / gPlayerStatus.hpMax) / 2;
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
                        work->anim                    = 3;
                        work->step                    = 5;
                        sc->anim.source.sets          = gGolemKnightBishopPlayerAnims;
                        sc->anim.animationId          = 3;
                        sc->anim.blend                = ANIMATION_BLEND_RESET;
                        sc->anim.blendFrames          = 0;
                        sc->anim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                        TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &sc->anim, 0);
                    } else {
                        work->timer = GOLEM_KNIGHT_BISHOP_GRAB_RECHECK;
                        taskMessageDispatch(player, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, Gp_PackPair(gGolemKnightBishopAttacks, 0), 0);
                        work->grabDamageTicks++;
                    }
                }
            }
            if (work->grabBreak != 0) {
                if (work->grabBreak == 1) {
                    if (work->auxTimer > 0 && work->counterattacking == 0) {
                        work->anim             = 0x15;
                        work->step             = 6;
                        work->grabReleaseTimer = 0;
                        if (work->appearSound != 0) {
                            SndEvt_EnqueueType7(work->appearSound, 1);
                            work->appearSound = 0;
                        }
                        work->fadeState = GOLEM_KNIGHT_BISHOP_FADE_FLICKER_START;
                    } else {
                        work->anim                   = 0xC;
                        work->step                   = 4;
                        work->timer                  = 0x69;
                        work->fadeState              = GOLEM_KNIGHT_BISHOP_FADE_VANISH;
                        work->translucencyFadeFrames = 0x4B;
                        work->grabReleaseTimer       = 0;
                        work->colorBlendFadeFrames   = 0x1E;
                        work->vanishSound            = gGolemKnightBishopPainCue | (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                        SndEvt_EnqueueType6(work->vanishSound, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                    }
                } else {
                    golemKnightBishopPickHitReaction(arg0, work->interruptDamage);
                    work->grabReleaseTimer = 0;
                }
                work->grabStage               = 2;
                work->grabBreak               = 0;
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
            if (work->animFrame < 0x5F) {
                val = -0xA;
            }
            work->forwardSpeed = val;
            timer              = work->timer - 1;
            work->timer        = timer;
            if (timer <= 0) {
                work->sequence = GOLEM_KNIGHT_BISHOP_SEQUENCE_IDLE;
                work->step     = 0;
            }
            golemKnightBishopHoldCueTimer(arg0);
            break;
        case 5:
            if (work->animFrame < 0x1A) {
                if (work->grabBreak != 0) {
                    work->anim                    = 0xC;
                    work->grabBreak               = 0;
                    sc->anim.source.sets          = gGolemKnightBishopPlayerAnims;
                    sc->anim.animationId          = 4;
                    sc->anim.blend                = ANIMATION_BLEND_RESET;
                    sc->anim.blendFrames          = 0;
                    sc->anim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                    TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &sc->anim, 0);
                    work->timer                  = 0x69;
                    work->fadeState              = GOLEM_KNIGHT_BISHOP_FADE_VANISH;
                    work->translucencyFadeFrames = 0x4B;
                    work->step                   = 4;
                    work->colorBlendFadeFrames   = 0x1E;
                    work->vanishSound            = gGolemKnightBishopPainCue | (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                    SndEvt_EnqueueType6(work->vanishSound, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                }
            } else if (work->animFrame == 0x1A) {
                ((GameActor*)player->work)->state = 0xA;
                work->step                        = 7;
                work->timer                       = 0;
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
            work->forwardSpeed = -0xA;
            golemKnightBishopHoldCueTimer(arg0);
            if (work->grabStage == 0) {
                work->forwardSpeed = 0;
                work->sequence     = GOLEM_KNIGHT_BISHOP_SEQUENCE_RECOVER;
                work->step         = 0;
            }
            break;
        case 7:
            sub = work->timer;
            switch (sub) {
                case 0:
                    CdCmd_EnqueueLoadFile(9, 0x1E, 3);
                    work->timer = 1;
                    break;
                case 1:
                    if ((CdCmd_IsIdle() & 0xFFFF) == 1) {
                        coord = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
                        SndEvt_EnqueueType6(SOUND_PLAYER_DEATH, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                        work->timer = 2;
                    }
                    break;
                case 2:
                    break;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(GolemKnightBishopGrabScratch));
}
