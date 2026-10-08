/* Part of the Rat library; see rat.h. */

/// Chooses the next wander-heading offset and its stored update countdown.
///
/// Borrows writable work; consumes two LCG draws in delay/heading order.
/// The delay draw stores 0..31; idle decrements before testing, so 0 and 1
/// both schedule another draw on the next update.
/// The heading draw supplies a magnitude 0..1023 and sign bit; target yaw
/// wraps to 0..4095 around the rat's current heading.
static __inline__ void _ratPickWanderHeading(RatWork* work)
{
    enum {
        RAT_WANDER_DELAY_MASK         = 31,
        RAT_WANDER_YAW_MAGNITUDE_MASK = ACTOR_TRANSFORM_ANGLE_TURN / 4 - 1,
        RAT_WANDER_YAW_SIGN_BIT       = ACTOR_TRANSFORM_ANGLE_TURN / 4
    };
    s32 wanderDelayRandom;
    s32 wanderYawRandom;
    s32 wanderYawOffset;

    work->turnRate    = RAT_WANDER_TURN_RATE;
    wanderDelayRandom = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    wanderYawRandom   = wanderDelayRandom * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    wanderYawOffset   = ((u32)wanderYawRandom >> 16) & RAT_WANDER_YAW_MAGNITUDE_MASK;
    gRandomLcgState   = wanderDelayRandom;
    work->wanderTimer = ((u32)wanderDelayRandom >> 16) & RAT_WANDER_DELAY_MASK;
    gRandomLcgState   = wanderYawRandom;
    if ((((u32)wanderYawRandom >> 16) & RAT_WANDER_YAW_SIGN_BIT) == 0) {
        wanderYawOffset = -wanderYawOffset;
    }
    work->targetYaw = ((u16)work->yaw + wanderYawOffset) & ACTOR_TRANSFORM_ANGLE_MASK;
}

/// Wanders in timed walk/run bursts until an attack is requested.
///
/// Requires live work/model and an `Enemy` whose placement row indexes the
/// carrier's chance tables. Duration draws index 0..15. Heading changes stay
/// within 1023 angle units of the current yaw; timer values count update calls.
/// An attack request starts a 60..91-frame approach and still ticks idle audio
/// on that transition frame.
static void _ratIdle(Task* actor)
{
    enum {
        RAT_IDLE_MOVE_ROLL_FRAMES     = 30,
        RAT_IDLE_APPROACH_BASE_FRAMES = 60,
    };

    RatWork*   work;
    TmdObject* model;
    GfxCoord*  rootCoord;
    s32        slowChanceRandom;
    s32        slowDurationRandom;
    s32        fastChanceRandom;
    s32        fastDurationRandom;
    s32        approachDurationRandom;
    s32        moveFrames;
    s32        soundId;
    s32        audioPan;

    work      = actor->work;
    model     = actor->extra.tmd;
    rootCoord = model->coords;
    switch (work->step) {
        case RAT_IDLE_STEP_REST:
            work->forwardSpeed      = 0;
            work->sensorBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            work->timer++;
            if (work->timer < RAT_IDLE_MOVE_ROLL_FRAMES) {
                break;
            }
            slowChanceRandom = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState  = slowChanceRandom;
            if ((s32)(((u32)slowChanceRandom >> 16) & 0xF) <
                gRatSlowMoveChance[((Enemy*)actor->spawnArg2.pointer)->place->rowIndex]) {
                work->animId    = RAT_ANIM_WALK;
                moveFrames      = gRatSlowMoveTimes[((u32)(slowDurationRandom = slowChanceRandom * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF];
                gRandomLcgState = slowDurationRandom;
                work->step      = RAT_IDLE_STEP_WALK;
                work->timer     = moveFrames;
                break;
            }
            fastChanceRandom = slowChanceRandom * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState  = fastChanceRandom;
            if ((s32)(((u32)fastChanceRandom >> 16) & 0xF) <
                gRatFastMoveChance[((Enemy*)actor->spawnArg2.pointer)->place->rowIndex]) {
                work->animId    = RAT_ANIM_RUN;
                moveFrames      = gRatFastMoveTimes[((u32)(fastDurationRandom = fastChanceRandom * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF];
                gRandomLcgState = fastDurationRandom;
                work->step      = RAT_IDLE_STEP_RUN;
                work->timer     = moveFrames;
                break;
            }
            work->timer = 0;
            break;
        case RAT_IDLE_STEP_WALK:
            work->forwardSpeed = RAT_WALK_SPEED;
            work->timer--;
            if (work->timer > 0) {
                break;
            }
            work->animId = RAT_ANIM_IDLE;
            work->timer  = 0;
            work->step   = RAT_IDLE_STEP_REST;
            break;
        case RAT_IDLE_STEP_RUN:
            work->forwardSpeed = RAT_RUN_SPEED;
            work->timer--;
            if (work->timer > 0) {
                break;
            }
            work->animId = RAT_ANIM_IDLE;
            work->timer  = 0;
            work->step   = RAT_IDLE_STEP_REST;
            break;
    }
    work->wanderTimer--;
    if (work->wanderTimer <= 0) {
        _ratPickWanderHeading(work);
    }
    // Attack entry still falls through to this update's idle-audio tick.
    if (work->attackRequested != 0) {
        work->mode             = RAT_MODE_ATTACK;
        work->attackRequested  = 0;
        work->step             = RAT_ATTACK_STEP_APPROACH;
        work->animId           = RAT_ANIM_RUN;
        approachDurationRandom = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->timer            = (((u32)approachDurationRandom >> 16) & 0x1F) + RAT_IDLE_APPROACH_BASE_FRAMES;
        soundId                = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << RAT_SOUND_PLACE_INDEX_SHIFT) | RAT_SOUND_ALERT;
        gRandomLcgState        = approachDurationRandom;
        audioPan               = (s8)worldCoordGetOriginAudioPan(rootCoord);
        sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(rootCoord));
    }
    _ratIdleSound(actor);
}
