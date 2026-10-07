/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Steps the root along local -Z at the walk animation's current rate.
///
/// work must be the task's live Mad Chaser work, with heading in 4096ths of a
/// turn and animRate in sixteenths of a frame. The normal-rate distance is -16
/// parent-coordinate units; rate scaling narrows it to s16 before multiplying
/// signed Q12 sine/cosine. Updates only X/Z translation and marks composition
/// dirty; animation playback and collision belong to the caller.
static __inline__ void _madChaserWalkApproachAdvanceRoot(Task* task, MadChaserWork* work)
{
    enum {
        MAD_CHASER_WALK_TRIG_PRODUCT_FRACTION_BITS = 16,
        MAD_CHASER_WALK_TRIG_FRACTION_BITS         = 12,
    };
    s16 heading;
    s16 stepDistance;

    stepDistance                          = _madChaserScaleByAnimRate(task, MAD_CHASER_WALK_DISTANCE_AT_NORMAL_RATE);
    heading                               = work->rotation.vy;
    task->extra.tmd->coords->coord.t[0]  += ((rsin(heading) << (MAD_CHASER_WALK_TRIG_PRODUCT_FRACTION_BITS - MAD_CHASER_WALK_TRIG_FRACTION_BITS)) * stepDistance) >> MAD_CHASER_WALK_TRIG_PRODUCT_FRACTION_BITS;
    task->extra.tmd->coords->coord.t[2]  += ((rcos(heading) << (MAD_CHASER_WALK_TRIG_PRODUCT_FRACTION_BITS - MAD_CHASER_WALK_TRIG_FRACTION_BITS)) * stepDistance) >> MAD_CHASER_WALK_TRIG_PRODUCT_FRACTION_BITS;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Approaches the tracked player until distance, facing and cooldown permit a leap.
///
/// Requires live model/enemy/work storage and a tracked player in the root's
/// parent frame. Only raises the walk's rate and turn step to the current
/// distance band; moving closer does not slow it. Steps along local -Z by
/// animRate units per frame and plays the walk cue on slot-1 boundary/jump/held
/// status. Advances within 2000 + leapRangeBonus units when cooldown is zero
/// or distance is below 1500, and bearing is strictly within +/-512 of the
/// forward direction (2048) in 4096 angle units per turn. The caller ticks
/// animation; movement dirties root composition and retains s16 step narrowing.
static void _madChaserWalkApproach(Task* task)
{
    MadChaserWork* work = task->work;
    s16            playerDistance;
    s16            minimumRate;
    s16            turnStep;
    s32            soundId;
    s32            audioPan;

    playerDistance = work->playerDist;
    // Keep the fastest band reached during this approach.
    if (playerDistance < MAD_CHASER_WALK_DISTANCE_BAND) {
        minimumRate = ANIMATION_RATE_ONE;
        turnStep    = MAD_CHASER_WALK_BASE_TURN_STEP;
    } else if (playerDistance < 2 * MAD_CHASER_WALK_DISTANCE_BAND) {
        turnStep    = MAD_CHASER_WALK_BASE_TURN_STEP + MAD_CHASER_WALK_TURN_BAND_STEP;
        minimumRate = ANIMATION_RATE_ONE + MAD_CHASER_WALK_RATE_BAND_STEP;
    } else if (playerDistance < 3 * MAD_CHASER_WALK_DISTANCE_BAND) {
        turnStep    = MAD_CHASER_WALK_BASE_TURN_STEP + 2 * MAD_CHASER_WALK_TURN_BAND_STEP;
        minimumRate = ANIMATION_RATE_ONE + 2 * MAD_CHASER_WALK_RATE_BAND_STEP;
    } else if (playerDistance < 4 * MAD_CHASER_WALK_DISTANCE_BAND) {
        turnStep    = MAD_CHASER_WALK_BASE_TURN_STEP + 3 * MAD_CHASER_WALK_TURN_BAND_STEP;
        minimumRate = ANIMATION_RATE_ONE + 3 * MAD_CHASER_WALK_RATE_BAND_STEP;
    } else if (playerDistance < 5 * MAD_CHASER_WALK_DISTANCE_BAND) {
        minimumRate = ANIMATION_RATE_ONE + 4 * MAD_CHASER_WALK_RATE_BAND_STEP;
        turnStep    = MAD_CHASER_WALK_BASE_TURN_STEP + 4 * MAD_CHASER_WALK_TURN_BAND_STEP;
    } else {
        minimumRate = MAD_CHASER_WALK_FAR_RATE;
        turnStep    = MAD_CHASER_WALK_FAR_TURN_STEP;
    }
    if (work->animRate < minimumRate) {
        work->animRate = minimumRate;
        work->turnStep = turnStep;
    }
    _madChaserTurnToPlayer(task, work->turnStep);
    _madChaserWalkApproachAdvanceRoot(task, work);
    if (_madChaserAnimHasBoundaryStatus(task)) {
        soundId  = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_MAD_CHASER, 1);
        audioPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    }
    // This unsigned wrap test excludes both facing-arc endpoints.
    if (work->playerDist < work->leapRangeBonus + MAD_CHASER_WALK_BASE_LEAP_DISTANCE && (work->playerDist < MAD_CHASER_WALK_CLOSE_LEAP_DISTANCE || work->leapCooldown == 0) &&
        (u16)(((work->playerBearing + ACTOR_TRANSFORM_ANGLE_TURN / 2) & ACTOR_TRANSFORM_ANGLE_MASK) - MAD_CHASER_WALK_LEAP_HALF_ARC) > ACTOR_TRANSFORM_ANGLE_TURN - 2 * MAD_CHASER_WALK_LEAP_HALF_ARC) {
        work->subState++;
    }
}
