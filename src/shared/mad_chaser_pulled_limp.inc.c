/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Holds a dead limp pull in its settle pose, except for a blast reaction.
///
/// Requires `work == task->work` and a current clip in 1..19. Clip 8 selects
/// clip 5 before the first leap or clip 6 afterwards; other clips use the
/// carrier's one-based settle table. A non-blast sets busy and requests a
/// four-frame blend at normal rate on the live work block. A blast only clears
/// busy, retaining the animation. Playback and task transitions belong to the
/// caller; this helper borrows the work and leaves state/subState intact.
static __inline__ void _madChaserPulledLimpRequestSettle(Task* task, MadChaserWork* work)
{
    s16 settleClip;

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

/// Accelerates the limp root toward the room's pull point until capture.
///
/// Requires live work, enemy/model storage and pullPoint in the root's parent
/// frame. Increments the u16 frame count and s16 acceleration/speed each call.
/// Before signed frame 90, horizontal distance is moveSpeed/64 per callback;
/// later it is moveSpeed/32, busy is set and Y eases by 1/16 of its remaining
/// offset. Direction and motion fields narrow to s16; coordinates use game units.
/// A 3D distance below 800 advances the sub-state before checking HP. Otherwise,
/// dead non-blast enemies request their settle clip blended over four normal-rate
/// frames; a dead blast clears busy. The caller updates animation and collision.
static void _madChaserPulledLimp(Task* task)
{
    enum {
        MAD_CHASER_LIMP_PULL_FAST_FRAME          = 90,
        MAD_CHASER_LIMP_PULL_SPEED_FRACTION_BITS = 6,
        MAD_CHASER_LIMP_PULL_Y_EASE_SHIFT        = 4,
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
    s16            pullHeading;

    model = task->extra.tmd;
    work  = task->work;
    enemy = task->spawnArg2.pointer;
    root  = model->coords;
    work->stateFrames++;
    work->moveAccel++;
    work->moveSpeed += work->moveAccel;
    if ((s16)work->stateFrames < MAD_CHASER_LIMP_PULL_FAST_FRAME) {
        s32 stepDistance = work->moveSpeed >> MAD_CHASER_LIMP_PULL_SPEED_FRACTION_BITS;

        liveRoot         = task->extra.tmd->coords;
        pullDirection.vx = work->pullPoint.vx - liveRoot->coord.t[0];
        pullDirection.vy = 0;
        pullDirection.vz = work->pullPoint.vz - liveRoot->coord.t[2];
        VectorNormalSS(&pullDirection, &pullDirection);
        pullHeading                           = ratan2(pullDirection.vx, pullDirection.vz);
        task->extra.tmd->coords->coord.t[0]  += ((rsin(pullHeading) << 4) * stepDistance) >> 16;
        task->extra.tmd->coords->coord.t[2]  += ((rcos(pullHeading) << 4) * stepDistance) >> 16;
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    } else {
        s32 stepDistance;

        work->busy       = 1;
        stepDistance     = work->moveSpeed >> (MAD_CHASER_LIMP_PULL_SPEED_FRACTION_BITS - 1);
        liveRoot         = task->extra.tmd->coords;
        pullDirection.vx = work->pullPoint.vx - liveRoot->coord.t[0];
        pullDirection.vy = 0;
        pullDirection.vz = work->pullPoint.vz - liveRoot->coord.t[2];
        VectorNormalSS(&pullDirection, &pullDirection);
        pullHeading                           = ratan2(pullDirection.vx, pullDirection.vz);
        task->extra.tmd->coords->coord.t[0]  += ((rsin(pullHeading) << 4) * stepDistance) >> 16;
        task->extra.tmd->coords->coord.t[2]  += ((rcos(pullHeading) << 4) * stepDistance) >> 16;
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        root->coord.t[1]                     += (work->pullPoint.vy - root->coord.t[1]) >> MAD_CHASER_LIMP_PULL_Y_EASE_SHIFT;
    }
    // Measure capture in 3D before allowing a dead enemy to settle.
    pullOffset.vx = root->coord.t[0] - work->pullPoint.vx;
    pullOffset.vy = root->coord.t[1] - work->pullPoint.vy;
    pullOffset.vz = root->coord.t[2] - work->pullPoint.vz;
    squareResult  = &squaredOffset;
    gte_ldlvl(&pullOffset);
    gte_sqr0();
    gte_stlvnl(squareResult);
    if (SquareRoot0(squaredOffset.vx + squaredOffset.vy + squaredOffset.vz) < MAD_CHASER_PULL_CAPTURE_DISTANCE) {
        work->subState++;
        return;
    }
    if (enemy->hp <= 0) {
        sndEvtRequestScriptStop(SOUND_MAD_CHASER_ALERT_CRY, SOUND_SCRIPT_STOP_KEEP_RELEASE);
        _madChaserPulledLimpRequestSettle(task, work);
    }
}
