/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Captures part 6's world position for the leap's brief anchoring interval.
///
/// Requires live work, a nine-part model and initialized view coordinates.
/// Composes the joint, removes the view transform and narrows its translation
/// to s16. Leaves the joint dirty for the subsequent animation/pinning pass.
static __inline__ void _madChaserLeapCaptureAnchor(Task* task, MadChaserWork* work)
{
    enum { MAD_CHASER_LEAP_ANCHOR_JOINT = 6 };
    MATRIX    anchorMatrix;
    GfxCoord* partCoords = task->extra.tmd->coords;
    SVECTOR*  anchorPos;

    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&gGfxViewCoord);
    partCoords[MAD_CHASER_LEAP_ANCHOR_JOINT].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&partCoords[MAD_CHASER_LEAP_ANCHOR_JOINT]);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &partCoords[MAD_CHASER_LEAP_ANCHOR_JOINT].workm, &anchorMatrix);
    anchorPos                                             = &work->leapAnchorPos;
    anchorPos->vx                                         = anchorMatrix.t[0];
    anchorPos->vy                                         = anchorMatrix.t[1];
    anchorPos->vz                                         = anchorMatrix.t[2];
    partCoords[MAD_CHASER_LEAP_ANCHOR_JOINT].composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Advances the leap's anchored windup, attack lunge and accelerating fall.
///
/// Requires initialized nine-part animation/work storage, a live enemy and root
/// parented to the view, and the launch height saved in moveStartPos.vy. Counts
/// callback frames with u16 wrapping, retaining the signed frame comparisons.
/// Hits can interrupt before frame 40; subsequent frames mark the move busy.
/// Captures part 6 in world space on frame 43 and pins it during frames 43..46.
/// Locks the lunge heading on frame 45, plays its positional cue on frame 46,
/// and steps -250 parent units along that heading on frames 45..53 with the
/// attack body enabled. From frame 47, the vertical acceleration grows by 30
/// per callback; moveSpeed and moveAccel narrow to s16 on each update.
///
/// On frames 45..48, playerDist below 369 starts clip 16 and jumps two sub-states
/// to rebound. Otherwise landing clamps the root to its launch Y, resets the
/// frame counter, starts clip 18 and advances one sub-state. Both transitions
/// blend over two normal-rate frames; the caller performs the animation tick.
static void _madChaserLeapAttack(Task* task)
{
    enum {
        MAD_CHASER_LEAP_COMMIT_FRAME            = 40,
        MAD_CHASER_LEAP_ANCHOR_START_FRAME      = 43,
        MAD_CHASER_LEAP_ANCHOR_END_FRAME        = 46,
        MAD_CHASER_LEAP_LOCK_HEADING_FRAME      = 45,
        MAD_CHASER_LEAP_SOUND_FRAME             = 46,
        MAD_CHASER_LEAP_LUNGE_START_FRAME       = 45,
        MAD_CHASER_LEAP_LUNGE_END_FRAME         = 53,
        MAD_CHASER_LEAP_REBOUND_START_FRAME     = 45,
        MAD_CHASER_LEAP_REBOUND_END_FRAME       = 48,
        MAD_CHASER_LEAP_FALL_START_FRAME        = 47,
        MAD_CHASER_LEAP_AIM_LIMIT               = 0x300,
        MAD_CHASER_LEAP_REBOUND_DISTANCE        = 369,
        MAD_CHASER_LEAP_LUNGE_DISTANCE          = -250,
        MAD_CHASER_LEAP_REBOUND_LAUNCH_SPEED    = -200,
        MAD_CHASER_LEAP_ACCEL_INCREMENT         = 30,
        MAD_CHASER_LEAP_REBOUND_CLIP            = 16,
        MAD_CHASER_LEAP_LAND_CLIP               = 18,
        MAD_CHASER_LEAP_TRANSITION_BLEND_FRAMES = 2,
        MAD_CHASER_LEAP_SOUND                   = SOUND_CHARACTER(SOUND_BANK_MAD_CHASER, 5),
    };
    MadChaserWork* work = task->work;
    GfxCoord*      root = task->extra.tmd->coords;
    s16            leapHeading;
    s32            soundId;
    s32            audioPan;
    s16            relativeLeapBearing;
    s16            stepDistance;

    if ((s16)++work->stateFrames < MAD_CHASER_LEAP_COMMIT_FRAME) {
        if (_madChaserTakeHitRequest(task)) {
            return;
        }
    } else {
        work->busy = 1;
    }
    // Hold the anchor joint in world space while the leap winds up.
    if ((s16)work->stateFrames == MAD_CHASER_LEAP_ANCHOR_START_FRAME) {
        _madChaserLeapCaptureAnchor(task, work);
    }
    if (work->stateFrames >= MAD_CHASER_LEAP_ANCHOR_START_FRAME && work->stateFrames <= MAD_CHASER_LEAP_ANCHOR_END_FRAME) {
        work->anchored = 1;
    } else {
        work->anchored = 0;
    }
    if ((s16)work->stateFrames == MAD_CHASER_LEAP_SOUND_FRAME) {
        soundId  = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | MAD_CHASER_LEAP_SOUND;
        audioPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    }
    // Reverse the player bearing to aim the negative-distance lunge.
    if ((s16)work->stateFrames == MAD_CHASER_LEAP_LOCK_HEADING_FRAME) {
        relativeLeapBearing = (work->playerBearing + ACTOR_TRANSFORM_ANGLE_HALF_TURN) & ACTOR_TRANSFORM_ANGLE_MASK;
        if (relativeLeapBearing < MAD_CHASER_LEAP_AIM_LIMIT) {
            work->leapHeading = (relativeLeapBearing + work->rotation.vy) & ACTOR_TRANSFORM_ANGLE_MASK;
        } else if (relativeLeapBearing >= ACTOR_TRANSFORM_ANGLE_TURN - MAD_CHASER_LEAP_AIM_LIMIT) {
            work->leapHeading = (relativeLeapBearing + work->rotation.vy) & ACTOR_TRANSFORM_ANGLE_MASK;
        } else {
            work->leapHeading = work->rotation.vy;
        }
    }
    if (work->stateFrames >= MAD_CHASER_LEAP_LUNGE_START_FRAME && work->stateFrames <= MAD_CHASER_LEAP_LUNGE_END_FRAME) {
        leapHeading                           = work->leapHeading;
        stepDistance                          = MAD_CHASER_LEAP_LUNGE_DISTANCE;
        task->extra.tmd->coords->coord.t[0]  += ((rsin(leapHeading) << 4) * stepDistance) >> 0x10;
        task->extra.tmd->coords->coord.t[2]  += ((rcos(leapHeading) << 4) * stepDistance) >> 0x10;
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        work->attackBody.flags               |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    } else {
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }
    // A nearby player selects rebound before the vertical fall update.
    if (work->stateFrames >= MAD_CHASER_LEAP_REBOUND_START_FRAME && work->stateFrames <= MAD_CHASER_LEAP_REBOUND_END_FRAME && work->playerDist < MAD_CHASER_LEAP_REBOUND_DISTANCE) {
        MadChaserWork* requestWork;

        work->moveAccel              = 0;
        work->moveSpeed              = MAD_CHASER_LEAP_REBOUND_LAUNCH_SPEED;
        requestWork                  = task->work;
        requestWork->animBlendFrames = MAD_CHASER_LEAP_TRANSITION_BLEND_FRAMES;
        requestWork->animRate        = ANIMATION_RATE_ONE;
        requestWork->animId          = MAD_CHASER_LEAP_REBOUND_CLIP;
        requestWork->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
        work->anchored               = 0;
        work->attackBody.flags      &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->subState              += 2;
        return;
    }
    // Move the grid sphere with the root, then settle at the launch height.
    if ((s16)work->stateFrames >= MAD_CHASER_LEAP_FALL_START_FRAME) {
        root->coord.t[1]      += work->moveSpeed;
        work->gridBody.pos.vy += work->moveSpeed;
        work->moveAccel       += MAD_CHASER_LEAP_ACCEL_INCREMENT;
        work->moveSpeed       += work->moveAccel;
        if (root->coord.t[1] >= work->moveStartPos.vy) {
            MadChaserWork* requestWork = task->work;

            requestWork->animBlendFrames = MAD_CHASER_LEAP_TRANSITION_BLEND_FRAMES;
            requestWork->animRate        = ANIMATION_RATE_ONE;
            requestWork->animId          = MAD_CHASER_LEAP_LAND_CLIP;
            requestWork->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
            root->coord.t[1]             = work->moveStartPos.vy;
            work->gridBody.pos.vy        = 0;
            work->stateFrames            = 0;
            work->subState++;
        }
    }
}
