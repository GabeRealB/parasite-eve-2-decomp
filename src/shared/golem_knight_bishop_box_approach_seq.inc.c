/* Part of the Knight/Bishop GOLEM library; see golem_knight_bishop.h. */

/// Breaks off the box approach into exposed recovery and disables its aim probe.
static inline void _golemKnightBishopInterruptBoxApproach(GolemKnightBishopWork* work)
{
    work->sequence           = GOLEM_KNIGHT_BISHOP_SEQUENCE_RECOVER;
    work->step               = GOLEM_KNIGHT_BISHOP_SEQUENCE_START;
    work->fadeState          = GOLEM_KNIGHT_BISHOP_FADE_FLICKER_START;
    work->forwardSpeed       = 0;
    work->aimBeamBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
}

/// Appears at the selected box region's post, aims, advances and charges.
///
/// `boxRegion` must select a live box entry in the task's room-region table.
/// The player leaving its heading corridor or sufficient interrupt damage ends
/// the approach. The charge's speed table must cover every animation frame
/// before the sequence exits; its raw u16 speeds narrow into signed s16 movement.
static void _golemKnightBishopBoxApproachSeq(Task* task)
{
    enum {
        GOLEM_KNIGHT_BISHOP_BOX_HEADING_TOLERANCE = 384,
        GOLEM_KNIGHT_BISHOP_CHARGE_CUE_FRAME      = 18,
        GOLEM_KNIGHT_BISHOP_CHARGE_ARM_FRAME      = 20,
        GOLEM_KNIGHT_BISHOP_CHARGE_DISARM_FRAME   = 32,
        GOLEM_KNIGHT_BISHOP_CHARGE_VANISH_FRAME   = 90,
    };
    enum {
        GOLEM_KNIGHT_BISHOP_BOX_APPEAR  = 0,
        GOLEM_KNIGHT_BISHOP_BOX_WAIT    = 1,
        GOLEM_KNIGHT_BISHOP_BOX_ADVANCE = 2,
        GOLEM_KNIGHT_BISHOP_BOX_CHARGE  = 3,
        GOLEM_KNIGHT_BISHOP_BOX_VANISH  = 4,
    };
    GolemKnightBishopOffsetScratch* scratch;
    s32                             step;
    GolemKnightBishopWork*          work;
    GfxCoord*                       root;
    s32                             sound;
    s32                             speedSpanIndex;
    s16                             headingDifference;
    s16                             headingError;
    s32                             advanceSpeed;
    s16                             framesLeft;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(GolemKnightBishopOffsetScratch);
    work    = task->work;
    step    = work->step;
    root    = task->extra.tmd->coords;
    switch (step) {
        case GOLEM_KNIGHT_BISHOP_BOX_APPEAR:
            root->coord.t[0]    = work->regions[work->boxRegion].x;
            root->coord.t[1]    = gPlayerStatus.coordMtx->t[1];
            root->coord.t[2]    = work->regions[work->boxRegion].z;
            scratch->operand.vx = 0;
            scratch->operand.vy = work->regions[work->boxRegion].param.heading;
            scratch->operand.vz = 0;
            RotMatrix(&scratch->operand, &root->coord);
            scratch->offset.vx = gPlayerStatus.coordMtx->t[0] - root->coord.t[0];
            scratch->offset.vz = gPlayerStatus.coordMtx->t[2] - root->coord.t[2];
            if ((s16)SquareRoot0(scratch->offset.vx * scratch->offset.vx + scratch->offset.vz * scratch->offset.vz) < 0xDAC) {
                work->anim  = GOLEM_KNIGHT_BISHOP_ANIM_STAND;
                work->step  = GOLEM_KNIGHT_BISHOP_BOX_WAIT;
                work->timer = 0x1E;
            } else {
                work->anim = GOLEM_KNIGHT_BISHOP_ANIM_ADVANCE;
                work->step = GOLEM_KNIGHT_BISHOP_BOX_ADVANCE;
            }
            work->fadeState              = GOLEM_KNIGHT_BISHOP_FADE_APPEAR;
            work->translucencyFadeFrames = GOLEM_KNIGHT_BISHOP_STANDARD_TRANSLUCENCY_FRAMES;
            work->colorBlendFadeFrames   = GOLEM_KNIGHT_BISHOP_STANDARD_COLOR_BLEND_FRAMES;
            work->appearSound            = gGolemKnightBishopApproachCue | ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
            sndEvtRequestScriptStart(work->appearSound, (s8)worldCoordGetOriginAudioPan(root), (s8)worldCoordGetOriginAudioDepth(root));
            sceneEngageBattle(1);
            if (work->hitCooldown == 0) {
                work->hurtBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                work->hurtBody.key    = work->actorId | WORLD_COLLISION_CONTACT_ENEMY_BODY;
            }
            work->auxTimer        = GOLEM_KNIGHT_BISHOP_AIM_TIME;
            work->interruptDamage = 0;
            work->reactionLock    = GOLEM_KNIGHT_BISHOP_REACTION_ATTACKING;
            break;
        case GOLEM_KNIGHT_BISHOP_BOX_WAIT:
            framesLeft  = work->timer - 1;
            work->timer = framesLeft;
            if (framesLeft <= 0) {
                work->anim = GOLEM_KNIGHT_BISHOP_ANIM_ADVANCE;
                work->step = GOLEM_KNIGHT_BISHOP_BOX_ADVANCE;
            }
            if (work->auxTimer > 0) {
                work->auxTimer--;
                _golemKnightBishopAimFromPart(task);
            }
            if (work->interruptDamage >= GOLEM_KNIGHT_BISHOP_HIT_WEIGHT) {
                _golemKnightBishopInterruptBoxApproach(work);
            }
            break;
        case GOLEM_KNIGHT_BISHOP_BOX_ADVANCE:
            advanceSpeed = 0;
            if (work->animFrame >= 8) {
                advanceSpeed = 0x78;
            }
            work->forwardSpeed = advanceSpeed;
            if (work->auxTimer > 0) {
                work->auxTimer--;
                _golemKnightBishopAimFromPart(task);
            }
            scratch->offset.vx = gPlayerStatus.coordMtx->t[0] - root->coord.t[0];
            scratch->offset.vz = gPlayerStatus.coordMtx->t[2] - root->coord.t[2];
            if ((s16)SquareRoot0(scratch->offset.vx * scratch->offset.vx + scratch->offset.vz * scratch->offset.vz) < 0xA8C) {
                work->anim               = GOLEM_KNIGHT_BISHOP_ANIM_CHARGE;
                work->step               = GOLEM_KNIGHT_BISHOP_BOX_CHARGE;
                work->aimBeamBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            } else {
                headingDifference = (ratan2((s16)scratch->offset.vx, (s16)scratch->offset.vz) & GOLEM_KNIGHT_BISHOP_YAW_MASK) - work->regions[work->boxRegion].param.heading;
                headingError      = (abs(headingDifference) >= GOLEM_KNIGHT_BISHOP_YAW_HALF_TURN) ? ((headingDifference > 0) ? GOLEM_KNIGHT_BISHOP_YAW_FULL_TURN - headingDifference : headingDifference + GOLEM_KNIGHT_BISHOP_YAW_FULL_TURN) : abs(headingDifference);
                if (headingError > GOLEM_KNIGHT_BISHOP_BOX_HEADING_TOLERANCE) {
                    work->anim                   = GOLEM_KNIGHT_BISHOP_ANIM_STAND;
                    work->step                   = GOLEM_KNIGHT_BISHOP_BOX_VANISH;
                    work->fadeState              = GOLEM_KNIGHT_BISHOP_FADE_VANISH;
                    work->translucencyFadeFrames = GOLEM_KNIGHT_BISHOP_STANDARD_TRANSLUCENCY_FRAMES;
                    work->colorBlendFadeFrames   = GOLEM_KNIGHT_BISHOP_STANDARD_COLOR_BLEND_FRAMES;
                    work->reactionLock           = GOLEM_KNIGHT_BISHOP_REACTION_UNLOCKED;
                    work->timer                  = work->translucencyFadeFrames + 0xA;
                    work->aimBeamBody.flags     &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                    work->vanishSound            = gGolemKnightBishopPainCue | ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                    sndEvtRequestScriptStart(work->vanishSound, (s8)worldCoordGetOriginAudioPan(root), (s8)worldCoordGetOriginAudioDepth(root));
                }
            }
            if (work->interruptDamage >= GOLEM_KNIGHT_BISHOP_HIT_WEIGHT) {
                _golemKnightBishopInterruptBoxApproach(work);
            }
            break;
        case GOLEM_KNIGHT_BISHOP_BOX_CHARGE:
            for (speedSpanIndex = 0; work->animFrame > gGolemKnightBishopFrameSteps[speedSpanIndex].lastFrame; speedSpanIndex++) {
            }
            work->forwardSpeed = gGolemKnightBishopFrameSteps[speedSpanIndex].forwardSpeed;
            if (work->animFrame == GOLEM_KNIGHT_BISHOP_CHARGE_CUE_FRAME) {
                sound = gGolemKnightBishopStrikeCue | ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                sndEvtRequestScriptStart(sound, (s8)worldCoordGetOriginAudioPan(root), (s8)worldCoordGetOriginAudioDepth(root));
            }
            if (work->animFrame == GOLEM_KNIGHT_BISHOP_CHARGE_ARM_FRAME) {
                work->strikeBody.coord  = task->extra.tmd->coords;
                work->strikeBody.pos.vy = -0x4B0;
                work->strikeBody.pos.vz = 0x1F4;
                work->strikeBody.pos.vx = 0;
                work->strikeBody.radius = 0x3E8;
                work->strikeBody.key    = damagePackAttackKey(gGolemKnightBishopAttacks, 2);
                work->strikeBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            }
            if (work->animFrame == GOLEM_KNIGHT_BISHOP_CHARGE_DISARM_FRAME) {
                work->reactionLock      = GOLEM_KNIGHT_BISHOP_REACTION_UNLOCKED;
                work->strikeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            if (work->animFrame == GOLEM_KNIGHT_BISHOP_CHARGE_VANISH_FRAME) {
                work->fadeState              = GOLEM_KNIGHT_BISHOP_FADE_VANISH;
                work->translucencyFadeFrames = GOLEM_KNIGHT_BISHOP_STANDARD_TRANSLUCENCY_FRAMES;
                work->colorBlendFadeFrames   = GOLEM_KNIGHT_BISHOP_STANDARD_COLOR_BLEND_FRAMES;
                work->step                   = GOLEM_KNIGHT_BISHOP_BOX_VANISH;
                work->timer                  = work->translucencyFadeFrames + 0xA;
                work->vanishSound            = gGolemKnightBishopPainCue | ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                sndEvtRequestScriptStart(work->vanishSound, (s8)worldCoordGetOriginAudioPan(root), (s8)worldCoordGetOriginAudioDepth(root));
            }
            break;
        case GOLEM_KNIGHT_BISHOP_BOX_VANISH:
            work->forwardSpeed = 0;
            framesLeft         = work->timer - 1;
            work->timer        = framesLeft;
            if (framesLeft <= 0) {
                work->sequence = GOLEM_KNIGHT_BISHOP_SEQUENCE_IDLE;
                work->step     = GOLEM_KNIGHT_BISHOP_SEQUENCE_START;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(GolemKnightBishopOffsetScratch);
}
