/* Part of the Knight/Bishop GOLEM library; see golem_knight_bishop.h. */

/// Runs the attack from the box region `boxRegion`. Step 0 plants the display
/// object on that region's post (`x`, `z`), faces it along `param.heading`,
/// starts the appearance with its sound and arms the hurt body, going to step
/// 1 when the player is within 0xDAC and to 2 otherwise. Steps 1 and 2 aim
/// the beam for `auxTimer` frames; step 2 walks in until the player is within
/// 0xA8C (step 3) or their bearing leaves the heading by more than 0x180
/// (step 4, vanishing). Either breaks off into the recover sequence once
/// `interruptDamage` reaches `GOLEM_KNIGHT_BISHOP_HIT_WEIGHT`. Step 3 is the
/// charge: `forwardSpeed` follows `gGolemKnightBishopFrameSteps`, `strikeBody`
/// is live on the root from frame 0x14 to 0x20, and frame 0x5A starts the
/// vanish. Step 4 counts `timer` down to the idle sequence.
void golemKnightBishopBoxApproachSeq(Task* arg0)
{
    u8*                             head;
    GolemKnightBishopOffsetScratch* sc;
    s32                             state;
    GolemKnightBishopWork*          work;
    GfxCoord*                       coord;
    s32                             pan;
    s32                             snd;
    s32                             i;
    s16                             diff;
    s16                             dist;
    s32                             adiff;
    s32                             val;
    s16                             timer;

    head                     = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(u8) = head - sizeof(GolemKnightBishopOffsetScratch);
    sc                       = (GolemKnightBishopOffsetScratch*)(head - sizeof(GolemKnightBishopOffsetScratch));
    work                     = arg0->work;
    state                    = work->step;
    coord                    = arg0->extra.tmd->coords;
    switch (state) {
        case 0:
            coord->coord.t[0] = work->regions[work->boxRegion].x;
            coord->coord.t[1] = gPlayerStatus.coordMtx->t[1];
            coord->coord.t[2] = work->regions[work->boxRegion].z;
            sc->operand.vx    = 0;
            sc->operand.vy    = work->regions[work->boxRegion].param.heading;
            sc->operand.vz    = 0;
            RotMatrix(&sc->operand, &coord->coord);
            sc->offset.vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
            sc->offset.vz = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            if ((s16)SquareRoot0(sc->offset.vx * sc->offset.vx + sc->offset.vz * sc->offset.vz) < 0xDAC) {
                work->anim  = 4;
                work->step  = 1;
                work->timer = 0x1E;
            } else {
                work->anim = 6;
                work->step = 2;
            }
            work->fadeState              = GOLEM_KNIGHT_BISHOP_FADE_APPEAR;
            work->translucencyFadeFrames = 0x14;
            work->colorBlendFadeFrames   = 0xA;
            work->appearSound            = gGolemKnightBishopApproachCue | (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
            sndEvtRequestScriptStart(work->appearSound, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            Gp_ArmStateF0(1);
            if (work->hitCooldown == 0) {
                work->hurtBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                work->hurtBody.key    = work->actorId | 0x30000;
            }
            work->auxTimer        = GOLEM_KNIGHT_BISHOP_AIM_TIME;
            work->interruptDamage = 0;
            work->reactionLock    = 1;
            break;
        case 1:
            timer       = work->timer - 1;
            work->timer = timer;
            if (timer <= 0) {
                work->anim = 6;
                work->step = 2;
            }
            if (work->auxTimer > 0) {
                work->auxTimer--;
                golemKnightBishopAimFromPart(arg0);
            }
            if (work->interruptDamage >= GOLEM_KNIGHT_BISHOP_HIT_WEIGHT) {
                work->sequence           = GOLEM_KNIGHT_BISHOP_SEQUENCE_RECOVER;
                work->step               = 0;
                work->fadeState          = GOLEM_KNIGHT_BISHOP_FADE_FLICKER_START;
                work->forwardSpeed       = 0;
                work->aimBeamBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            }
            break;
        case 2:
            val = 0;
            if (work->animFrame >= 8) {
                val = 0x78;
            }
            work->forwardSpeed = val;
            if (work->auxTimer > 0) {
                work->auxTimer--;
                golemKnightBishopAimFromPart(arg0);
            }
            sc->offset.vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
            sc->offset.vz = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            if ((s16)SquareRoot0(sc->offset.vx * sc->offset.vx + sc->offset.vz * sc->offset.vz) < 0xA8C) {
                work->anim               = 7;
                work->step               = 3;
                work->aimBeamBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            } else {
                diff = (ratan2((s16)sc->offset.vx, (s16)sc->offset.vz) & 0xFFF) - work->regions[work->boxRegion].param.heading;
                dist = (abs(diff) >= 0x800) ? ((diff > 0) ? 0x1000 - diff : diff + 0x1000) : abs(diff);
                if (dist > 0x180) {
                    work->anim                   = 4;
                    work->step                   = 4;
                    work->fadeState              = GOLEM_KNIGHT_BISHOP_FADE_VANISH;
                    work->translucencyFadeFrames = 0x14;
                    work->colorBlendFadeFrames   = 0xA;
                    work->reactionLock           = 0;
                    work->timer                  = work->translucencyFadeFrames + 0xA;
                    work->aimBeamBody.flags     &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                    work->vanishSound            = gGolemKnightBishopPainCue | (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                    sndEvtRequestScriptStart(work->vanishSound, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                }
            }
            if (work->interruptDamage >= GOLEM_KNIGHT_BISHOP_HIT_WEIGHT) {
                work->sequence           = GOLEM_KNIGHT_BISHOP_SEQUENCE_RECOVER;
                work->step               = 0;
                work->fadeState          = GOLEM_KNIGHT_BISHOP_FADE_FLICKER_START;
                work->forwardSpeed       = 0;
                work->aimBeamBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            }
            break;
        case 3:
            for (i = 0; work->animFrame > gGolemKnightBishopFrameSteps[i].lastFrame; i++) {
            }
            work->forwardSpeed = gGolemKnightBishopFrameSteps[i].forwardSpeed;
            if (work->animFrame == 0x12) {
                snd = gGolemKnightBishopStrikeCue | (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                sndEvtRequestScriptStart(snd, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->animFrame == 0x14) {
                work->strikeBody.coord  = arg0->extra.tmd->coords;
                work->strikeBody.pos.vy = -0x4B0;
                work->strikeBody.pos.vz = 0x1F4;
                work->strikeBody.pos.vx = 0;
                work->strikeBody.radius = 0x3E8;
                work->strikeBody.key    = Gp_PackPair(gGolemKnightBishopAttacks, 2);
                work->strikeBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            }
            if (work->animFrame == 0x20) {
                work->reactionLock      = 0;
                work->strikeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            if (work->animFrame == 0x5A) {
                work->fadeState              = GOLEM_KNIGHT_BISHOP_FADE_VANISH;
                work->translucencyFadeFrames = 0x14;
                work->colorBlendFadeFrames   = 0xA;
                work->step                   = 4;
                work->timer                  = work->translucencyFadeFrames + 0xA;
                work->vanishSound            = gGolemKnightBishopPainCue | (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                sndEvtRequestScriptStart(work->vanishSound, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            }
            break;
        case 4:
            work->forwardSpeed = 0;
            timer              = work->timer - 1;
            work->timer        = timer;
            if (timer <= 0) {
                work->sequence = GOLEM_KNIGHT_BISHOP_SEQUENCE_IDLE;
                work->step     = 0;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(GolemKnightBishopOffsetScratch));
}
