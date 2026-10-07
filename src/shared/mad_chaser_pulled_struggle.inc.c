/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Holds a dead resisting pull in its settle pose, except for a blast reaction.
///
/// Requires `work == task->work` and a current clip in 1..19. Clip 8 selects
/// clip 5 before the first leap or clip 6 afterwards; other clips use the
/// carrier's one-based settle table. A non-blast sets busy and requests a
/// four-frame blend at normal rate on the live work block. A blast only clears
/// busy, retaining the animation. Playback and task transitions belong to the
/// caller; this helper borrows the work and leaves state/subState intact.
static __inline__ void _madChaserPulledStruggleRequestSettle(Task* task, MadChaserWork* work)
{
    if (work->hitReaction != MAD_CHASER_HIT_REACTION_BLAST) {
        work->busy = 1;
        if (work->animId == MAD_CHASER_PULL_LEAP_CLIP) {
            if (work->hasLeaped == 0) {
                MadChaserWork* requestWork = task->work;

                requestWork->animBlendFrames = MAD_CHASER_PULL_SETTLE_BLEND_FRAMES;
                requestWork->animRate        = ANIMATION_RATE_ONE;
                requestWork->animId          = MAD_CHASER_PULL_LOW_SETTLE_CLIP;
                requestWork->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
            } else {
                MadChaserWork* requestWork = task->work;

                requestWork->animBlendFrames = MAD_CHASER_PULL_SETTLE_BLEND_FRAMES;
                requestWork->animRate        = ANIMATION_RATE_ONE;
                requestWork->animId          = MAD_CHASER_PULL_UPRIGHT_SETTLE_CLIP;
                requestWork->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
            }
        } else {
            MadChaserWork* requestWork;
            s16            settleClip;

            settleClip                   = gMadChaserSettleAnims[work->animId - 1];
            requestWork                  = task->work;
            requestWork->animBlendFrames = MAD_CHASER_PULL_SETTLE_BLEND_FRAMES;
            requestWork->animRate        = ANIMATION_RATE_ONE;
            requestWork->animId          = settleClip;
            requestWork->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
        }
    } else {
        work->busy = 0;
    }
}

/// Resists a scripted pull while accelerating toward the room's capture point.
///
/// Requires live work, enemy/model storage and pullPoint in the root's parent
/// frame. While alive, ramps the rate from 16 through 31 over four counts per unit,
/// then switches to 64 (rates are in sixteenths of a frame). It
/// turns by 24 of 4096 angle units outside a +/-256 dead band, and backs away by
/// animRate game units. Also accelerates toward the point by moveSpeed/64 each
/// callback, with s16 motion/offset narrowing and a wrapping u16 frame counter.
/// After signed frame 120, sets busy and eases Y by 1/16 within 3000 units or
/// 1/32 farther away; earlier death uses 1/32. Earlier live slot-1 boundary/jump/
/// held status plays the resistance cue. A 3D distance below 800 advances before
/// the HP check; otherwise dead non-blast enemies request a four-frame settle
/// blend and dead blasts clear busy. The caller ticks animation and collision.
static void _madChaserPulledStruggle(Task* task)
{
    enum {
        MAD_CHASER_STRUGGLE_PULL_MAX_RATE            = 64,
        MAD_CHASER_STRUGGLE_PULL_TURN_DEAD_BAND      = 256,
        MAD_CHASER_STRUGGLE_PULL_TURN_STEP           = 24,
        MAD_CHASER_STRUGGLE_PULL_RESIST_DISTANCE     = -16,
        MAD_CHASER_STRUGGLE_PULL_RATE_FRACTION_BITS  = 4,
        MAD_CHASER_STRUGGLE_PULL_SPEED_FRACTION_BITS = 6,
        MAD_CHASER_STRUGGLE_PULL_Y_EASE_FRAME        = 120,
        MAD_CHASER_STRUGGLE_PULL_FAST_Y_DISTANCE     = 3000,
        MAD_CHASER_STRUGGLE_PULL_FAST_Y_SHIFT        = 4,
        MAD_CHASER_STRUGGLE_PULL_SLOW_Y_SHIFT        = 5,
        MAD_CHASER_STRUGGLE_PULL_SOUND               = SOUND_CHARACTER(SOUND_BANK_MAD_CHASER, 1),
    };
    TmdObject*     model;
    MadChaserWork* work;
    Enemy*         enemy;
    GfxCoord*      root;
    GfxCoord*      liveRoot;
    VECTOR         pullOffset;
    SVECTOR        pullDirection;
    VECTOR         squaredOffset;
    VECTOR*        squareResult;
    s16            moveHeading;
    s32            pullDistance;
    s32            soundId;
    s32            audioPan;

    model = task->extra.tmd;
    work  = task->work;
    root  = model->coords;
    enemy = task->spawnArg2.pointer;
    work->stateFrames++;
    if (enemy->hp > 0) {
        MadChaserWork* headingWork;
        s32            headingError;
        s32            distanceAtNormalRate;
        s32            stepDistance;

        if ((u32)(work->stateScratch >> 1) < MAD_CHASER_STRUGGLE_PULL_MAX_RATE) {
            work->animRate = work->stateScratch >> 2;
            work->stateScratch++;
        } else {
            work->animRate = MAD_CHASER_STRUGGLE_PULL_MAX_RATE;
        }
        headingWork      = task->work;
        liveRoot         = task->extra.tmd->coords;
        pullDirection.vx = work->pullPoint.vx - liveRoot->coord.t[0];
        pullDirection.vy = 0;
        pullDirection.vz = work->pullPoint.vz - liveRoot->coord.t[2];
        VectorNormalSS(&pullDirection, &pullDirection);
        headingError = (((u16)headingWork->rotation.vy - ratan2(pullDirection.vx, pullDirection.vz)) << 20) >> 20;
        if (headingError > MAD_CHASER_STRUGGLE_PULL_TURN_DEAD_BAND) {
            headingWork->rotation.vy -= MAD_CHASER_STRUGGLE_PULL_TURN_STEP;
        } else if (headingError < -MAD_CHASER_STRUGGLE_PULL_TURN_DEAD_BAND) {
            headingWork->rotation.vy += MAD_CHASER_STRUGGLE_PULL_TURN_STEP;
        }
        moveHeading                           = work->rotation.vy;
        distanceAtNormalRate                  = MAD_CHASER_STRUGGLE_PULL_RESIST_DISTANCE;
        stepDistance                          = ((((MadChaserWork*)task->work)->animRate * distanceAtNormalRate) << (16 - MAD_CHASER_STRUGGLE_PULL_RATE_FRACTION_BITS)) >> 16;
        task->extra.tmd->coords->coord.t[0]  += ((rsin(moveHeading) << 4) * stepDistance) >> 16;
        task->extra.tmd->coords->coord.t[2]  += ((rcos(moveHeading) << 4) * stepDistance) >> 16;
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    // The pull accelerates even while the live enemy pushes away.
    work->moveAccel++;
    work->moveSpeed += work->moveAccel;
    {
        s32 stepDistance = work->moveSpeed >> MAD_CHASER_STRUGGLE_PULL_SPEED_FRACTION_BITS;

        liveRoot         = task->extra.tmd->coords;
        pullDirection.vx = work->pullPoint.vx - liveRoot->coord.t[0];
        pullDirection.vy = 0;
        pullDirection.vz = work->pullPoint.vz - liveRoot->coord.t[2];
        VectorNormalSS(&pullDirection, &pullDirection);
        moveHeading                           = ratan2(pullDirection.vx, pullDirection.vz);
        task->extra.tmd->coords->coord.t[0]  += ((rsin(moveHeading) << 4) * stepDistance) >> 16;
        task->extra.tmd->coords->coord.t[2]  += ((rcos(moveHeading) << 4) * stepDistance) >> 16;
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    // Measure capture in 3D before allowing a dead enemy to settle.
    pullOffset.vx = root->coord.t[0] - work->pullPoint.vx;
    pullOffset.vy = root->coord.t[1] - work->pullPoint.vy;
    pullOffset.vz = root->coord.t[2] - work->pullPoint.vz;
    squareResult  = &squaredOffset;
    gte_ldlvl(&pullOffset);
    gte_sqr0();
    gte_stlvnl(squareResult);
    pullDistance = SquareRoot0(squaredOffset.vx + squaredOffset.vy + squaredOffset.vz);
    if ((s16)work->stateFrames > MAD_CHASER_STRUGGLE_PULL_Y_EASE_FRAME) {
        if (pullDistance <= MAD_CHASER_STRUGGLE_PULL_FAST_Y_DISTANCE) {
            work->busy        = 1;
            root->coord.t[1] += (work->pullPoint.vy - root->coord.t[1]) >> MAD_CHASER_STRUGGLE_PULL_FAST_Y_SHIFT;
        } else {
            work->busy        = 1;
            root->coord.t[1] += (work->pullPoint.vy - root->coord.t[1]) >> MAD_CHASER_STRUGGLE_PULL_SLOW_Y_SHIFT;
        }
    } else if (enemy->hp > 0) {
        if (_madChaserAnimHasBoundaryStatusInline(task)) {
            soundId  = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | MAD_CHASER_STRUGGLE_PULL_SOUND;
            audioPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
            sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        }
    } else {
        work->busy        = 1;
        root->coord.t[1] += (work->pullPoint.vy - root->coord.t[1]) >> MAD_CHASER_STRUGGLE_PULL_SLOW_Y_SHIFT;
    }
    if (pullDistance < MAD_CHASER_PULL_CAPTURE_DISTANCE) {
        work->subState++;
        return;
    }
    if (enemy->hp <= 0) {
        _madChaserPulledStruggleRequestSettle(task, work);
    }
}
