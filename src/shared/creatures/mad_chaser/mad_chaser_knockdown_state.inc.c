/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Dispatches the current knockdown step, then accepts hits for an upright stance.
///
/// Requires live Mad Chaser work in combat behavior 9, with subState in 0..2.
/// Copies the complete three-callback table before indexing as s16. After the
/// step, saved stance 1 permits a pending status or knockdown reaction to replace
/// the behavior, even if recovery just changed it. Other stances do not consume
/// requests here. Animation playback belongs to the combat frame callback.
static void _madChaserKnockdownState(Task* task)
{
    MadChaserWork* work;
    TaskFuncTable3 steps;

    work  = task->work;
    steps = _gMadChaserKnockdownSteps;
    steps.funcs[(s16)work->subState](task);
    // The dispatched step may have selected a new behavior; still process this hit.
    if (work->stateScratch == MAD_CHASER_STANCE_UPRIGHT) {
        _madChaserTakeKnockdownRequest(task);
    }
}

/// Dispatches one step of the scripted pull within combat behavior 10.
///
/// Requires live Mad Chaser work with subState in 0..5. Copies the complete six-
/// callback table before indexing as s16: start, reaction, resisting pull,
/// capture, limp pull and capture. The selected step owns its state changes;
/// the combat frame callback subsequently ticks animation and resolves collision.
static void _madChaserPullState(Task* task)
{
    MadChaserWork* work;
    TaskFuncTable6 steps;

    work  = task->work;
    steps = _gMadChaserPullSteps;
    steps.funcs[(s16)work->subState](task);
}

/// Starts the buildup-status hold with an eight-frame blend into clip 12.
///
/// Requires live animation/work storage and the status entry sub-state. The
/// enemy's buildup reaction must already be started. Requests normal-rate
/// playback and advances to the hold; animation ticking belongs to the caller.
static void _madChaserStatusHoldStart(Task* task)
{
    enum {
        MAD_CHASER_STATUS_ENTRY_CLIP         = 12,
        MAD_CHASER_STATUS_ENTRY_BLEND_FRAMES = 8,
    };
    MadChaserWork* work = task->work;

    work->animBlendFrames = MAD_CHASER_STATUS_ENTRY_BLEND_FRAMES;
    work->animRate        = ANIMATION_RATE_ONE;
    work->animId          = MAD_CHASER_STATUS_ENTRY_CLIP;
    work->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
    work->subState        = work->subState + 1;
}

/// Advances the enemy's buildup-status reaction while maintaining its hold pose.
///
/// Requires live work, initialized animation storage, and an enemy with a
/// started buildup reaction, live parameters and grade in 0..3. Slot-1
/// boundary/jump/held status requests a four-frame normal-rate blend into
/// clip 11. Ticks buildup once each call, then selects combat walk at sub-state
/// zero when it expires. Does not clear the buildup flag or tick animation.
static void _madChaserStatusHold(Task* task)
{
    enum {
        MAD_CHASER_STATUS_HOLD_CLIP         = 11,
        MAD_CHASER_STATUS_HOLD_BLEND_FRAMES = 4,
    };
    MadChaserWork* requestWork;

    if (_madChaserAnimHasBoundaryStatus(task)) {
        requestWork                  = task->work;
        requestWork->animBlendFrames = MAD_CHASER_STATUS_HOLD_BLEND_FRAMES;
        requestWork->animRate        = ANIMATION_RATE_ONE;
        requestWork->animId          = MAD_CHASER_STATUS_HOLD_CLIP;
        requestWork->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
    }
    if (damageTickEnemyBuildup(task->spawnArg2.pointer) != 0) {
        _madChaserSetBehaviorState(task, MAD_CHASER_COMBAT_STATE_WALK);
    }
}

/// Saves the interrupted clip's stance and starts its knockdown pose.
///
/// Requires live work and a clip in 1..19, indexing the carrier's stance table
/// at clip minus one. Stance 1 is upright; all other values select the low pose.
/// Blends to clip 6 or 5 over six normal-rate frames and advances the sub-state.
static void _madChaserKnockdownStart(Task* task)
{
    enum {
        MAD_CHASER_KNOCKDOWN_UPRIGHT_STANCE = 1,
        MAD_CHASER_KNOCKDOWN_UPRIGHT_CLIP   = 6,
        MAD_CHASER_KNOCKDOWN_LOW_CLIP       = 5,
        MAD_CHASER_KNOCKDOWN_BLEND_FRAMES   = 6,
    };
    MadChaserWork* work;

    work               = task->work;
    work->stateScratch = _gMadChaserAnimStance[work->animId - 1];
    if (work->stateScratch == MAD_CHASER_KNOCKDOWN_UPRIGHT_STANCE) {
        MadChaserWork* requestWork = task->work;

        requestWork->animBlendFrames = MAD_CHASER_KNOCKDOWN_BLEND_FRAMES;
        requestWork->animRate        = ANIMATION_RATE_ONE;
        requestWork->animId          = MAD_CHASER_KNOCKDOWN_UPRIGHT_CLIP;
        requestWork->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
    } else {
        MadChaserWork* requestWork = task->work;

        requestWork->animBlendFrames = MAD_CHASER_KNOCKDOWN_BLEND_FRAMES;
        requestWork->animRate        = ANIMATION_RATE_ONE;
        requestWork->animId          = MAD_CHASER_KNOCKDOWN_LOW_CLIP;
        requestWork->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
    }
    work->subState++;
}

/// Starts recovery once the knockdown animation reports a boundary, jump or hold.
///
/// Saved stance 1 recovers upright with clip 7 and a 50-frame blend; all other
/// values recover low with clip 1 and a 30-frame blend. Both play at normal rate
/// and advance the sub-state. Requires initialized animation storage and the
/// stance saved by `_madChaserKnockdownStart`; busy and task state are retained.
static void _madChaserKnockdownRise(Task* task)
{
    enum {
        MAD_CHASER_KNOCKDOWN_RISE_UPRIGHT_STANCE       = 1,
        MAD_CHASER_KNOCKDOWN_RISE_UPRIGHT_CLIP         = 7,
        MAD_CHASER_KNOCKDOWN_RISE_LOW_CLIP             = 1,
        MAD_CHASER_KNOCKDOWN_RISE_UPRIGHT_BLEND_FRAMES = 50,
        MAD_CHASER_KNOCKDOWN_RISE_LOW_BLEND_FRAMES     = 30,
    };
    MadChaserWork* work;
    MadChaserWork* lowWork;
    MadChaserWork* uprightWork;

    work = task->work;
    if (_madChaserAnimHasBoundaryStatus(task) != 0) {
        if (work->stateScratch == MAD_CHASER_KNOCKDOWN_RISE_UPRIGHT_STANCE) {
            uprightWork                  = task->work;
            uprightWork->animBlendFrames = MAD_CHASER_KNOCKDOWN_RISE_UPRIGHT_BLEND_FRAMES;
            uprightWork->animRate        = ANIMATION_RATE_ONE;
            uprightWork->animId          = MAD_CHASER_KNOCKDOWN_RISE_UPRIGHT_CLIP;
            uprightWork->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
        } else {
            lowWork                  = task->work;
            lowWork->animBlendFrames = MAD_CHASER_KNOCKDOWN_RISE_LOW_BLEND_FRAMES;
            lowWork->animRate        = ANIMATION_RATE_ONE;
            lowWork->animId          = MAD_CHASER_KNOCKDOWN_RISE_LOW_CLIP;
            lowWork->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
        }
        work->subState = work->subState + 1;
    }
}

/// Resumes combat walking or alert after the recovery animation's boundary status.
///
/// Saved stance 1 selects combat walk; all other values select combat alert.
/// Resets the behavior sub-state, retaining task state, animation and counters.
/// Requires initialized work and the stance saved by `_madChaserKnockdownStart`.
static void _madChaserKnockdownEnd(Task* task)
{
    enum { MAD_CHASER_KNOCKDOWN_END_UPRIGHT_STANCE = 1 };
    MadChaserWork* work = task->work;

    if (_madChaserAnimHasBoundaryStatus(task)) {
        if (work->stateScratch == MAD_CHASER_KNOCKDOWN_END_UPRIGHT_STANCE) {
            _madChaserSetBehaviorState(task, MAD_CHASER_COMBAT_STATE_WALK);
        } else {
            _madChaserSetBehaviorState(task, MAD_CHASER_COMBAT_STATE_ALERT);
        }
    }
}

/// Steps the root along local -Z at the walk animation's current rate.
///
/// work must be the task's live Mad Chaser work, with heading in 4096ths of a
/// turn and animRate in sixteenths of a frame. The normal-rate distance is -16
/// parent-coordinate units; rate scaling narrows it to s16 before multiplying
/// signed Q12 sine/cosine. Updates only X/Z translation and marks composition
/// dirty; animation playback and collision belong to the caller.
static inline void _madChaserWalkFinishAdvanceRoot(Task* task, MadChaserWork* work)
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

/// Finishes the walk cycle before handing the combat behavior to the leap.
///
/// Requires live work/model storage and the last tracked player offset in the
/// root's parent frame. Turns by 16 of 4096 angle units and steps along local
/// -Z by animRate parent-coordinate units, narrowed to s16, dirtying composition.
/// Slot-1 boundary/jump/held status selects leap at sub-state zero; distance and
/// cooldown are not rechecked. The caller ticks animation and collision.
static void _madChaserWalkFinish(Task* task)
{
    MadChaserWork* work = task->work;

    _madChaserTurnToPlayer(task, MAD_CHASER_WALK_BASE_TURN_STEP);
    _madChaserWalkFinishAdvanceRoot(task, work);
    if (_madChaserAnimHasBoundaryStatus(task)) {
        _madChaserSetBehaviorState(task, MAD_CHASER_COMBAT_STATE_LEAP);
    }
}

/// Starts the leap windup and saves the height to which it will land.
///
/// Requires live work and root coordinates in the world frame under the view.
/// Saves root Y narrowed to s16, blends clip 8 over eight normal-rate frames,
/// clears the frame counter/acceleration and seeds vertical speed at -300 parent
/// units per callback. Marks hasLeaped, clears busy/anchored and advances the
/// sub-state. Animation playback and the subsequent motion belong to the caller.
static void _madChaserStartLeap(Task* task)
{
    enum {
        MAD_CHASER_LEAP_WINDUP_CLIP            = 8,
        MAD_CHASER_LEAP_WINDUP_BLEND_FRAMES    = MAD_CHASER_LEAP_WINDUP_CLIP,
        MAD_CHASER_LEAP_INITIAL_VERTICAL_SPEED = -300,
    };
    MadChaserWork* work;
    MadChaserWork* requestWork;
    s16            requestValue;

    work                  = task->work;
    work->moveStartPos.vy = task->extra.tmd->coords->coord.t[1];
    requestWork           = task->work;
    // Reuse the request value to retain the initializer's emitted store sequence.
    requestValue                 = MAD_CHASER_LEAP_WINDUP_BLEND_FRAMES;
    requestWork->animBlendFrames = requestValue;
    requestWork->animId          = requestValue;
    requestWork->animRate        = ANIMATION_RATE_ONE;
    requestValue                 = MAD_CHASER_ANIM_REQUEST_BLEND;
    requestWork->animRequest     = requestValue;
    work->stateFrames            = 0;
    work->moveAccel              = 0;
    work->moveSpeed              = MAD_CHASER_LEAP_INITIAL_VERTICAL_SPEED;
    work->hasLeaped              = requestValue;
    work->busy                   = 0;
    work->anchored               = 0;
    work->subState               = work->subState + 1;
}

/// Finishes the rebound landing and returns to combat walking.
///
/// Requires live work and enemy/model storage with stateFrames reset on landing.
/// Clears busy each call and plays the positional landing cue on frame 1. A
/// slot-1 boundary, jump or held pose stores a 90..217 combat-tick leap cooldown
/// from one random draw, selects walk behavior and resets its sub-state. The
/// caller ticks animation; the u16 frame counter retains its signed comparison.
static void _madChaserLeapLand(Task* task)
{
    enum {
        MAD_CHASER_LEAP_COOLDOWN_MIN_FRAMES  = 90,
        MAD_CHASER_LEAP_COOLDOWN_RANDOM_MASK = 127,
        MAD_CHASER_LEAP_LANDING_SOUND        = SOUND_CHARACTER(SOUND_BANK_MAD_CHASER, 4),
    };
    MadChaserWork* work;
    s32            soundId;
    s32            audioPan;
    u32            randomState;

    work       = task->work;
    work->busy = 0;
    if ((s16)++work->stateFrames == 1) {
        soundId  = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | MAD_CHASER_LEAP_LANDING_SOUND;
        audioPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    }
    if (_madChaserAnimHasBoundaryStatus(task) != 0) {
        randomState        = RANDOM_LCG_NEXT(gRandomLcgState);
        gRandomLcgState    = randomState;
        work->leapCooldown = ((randomState >> 16) & MAD_CHASER_LEAP_COOLDOWN_RANDOM_MASK) + MAD_CHASER_LEAP_COOLDOWN_MIN_FRAMES;
        _madChaserSetBehaviorState(task, MAD_CHASER_COMBAT_STATE_WALK);
    }
}

/// Starts the alert cry animation and sounds it while the enemy is alive.
///
/// Starts clip 9 with a four-normal-frame blend, resets the state counter and
/// advances to the wait step. The sound uses bank 0x402C, entry 2, tagged with
/// this enemy's placement index (0..15), with signed-byte spatial pan and depth.
/// Requires live work, an Enemy spawn argument and loaded model/animation data.
static void _madChaserAlertCry(Task* task)
{
    enum {
        MAD_CHASER_ALERT_CRY_ANIM  = 9,
        MAD_CHASER_ALERT_CRY_SOUND = 0x402C0002,
    };
    MadChaserWork* work;
    Enemy*         enemy;
    Enemy*         soundEnemy;
    s32            soundId;
    s32            panOffset;

    work                  = task->work;
    enemy                 = task->spawnArg2.pointer;
    work->animBlendFrames = 4;
    work->animRate        = ANIMATION_RATE_ONE;
    work->animId          = MAD_CHASER_ALERT_CRY_ANIM;
    work->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
    work->stateFrames     = 0;
    work->subState++;
    if (enemy->hp > 0) {
        soundEnemy = task->spawnArg2.pointer;
        soundId    = ((soundEnemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | MAD_CHASER_ALERT_CRY_SOUND;
        panOffset  = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, panOffset, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    }
}

/// Holds the alert until the previous frame count reaches 81.
///
/// Post-increments the unsigned halfword counter, testing its old value as s16.
/// Starting from zero takes 82 calls to advance the sub-state; the counter is
/// retained. Requires live Mad Chaser work.
static void _madChaserAlertWait(Task* task)
{
    enum {
        MAD_CHASER_ALERT_WAIT_THRESHOLD = 81,
    };
    u16            previousFrames;
    MadChaserWork* work;

    work              = task->work;
    previousFrames    = work->stateFrames;
    work->stateFrames = previousFrames + 1;
    if ((s16)previousFrames >= MAD_CHASER_ALERT_WAIT_THRESHOLD) {
        work->subState = work->subState + 1;
    }
}

/// Releases this enemy's shared alert claim at an animation boundary.
///
/// Slot 1 reaching a boundary, control jump or settled pose releases only a
/// claim with this enemy's placement index. Starts clip 15 with an eight-frame
/// blend at normal rate and advances the alert sub-state; retains stateFrames.
static void _madChaserAlertRelease(Task* task)
{
    enum {
        MAD_CHASER_ALERT_RELEASE_ANIM = 15,
    };
    MadChaserWork* work;
    MadChaserWork* animationWork;

    work = task->work;
    if ((_madChaserAnimHasBoundaryStatus(task) << 0x10) != 0) {
        _madChaserSetAlertHold(task, 0);

        animationWork                  = task->work;
        animationWork->animBlendFrames = 8;
        animationWork->animRate        = ANIMATION_RATE_ONE;
        animationWork->animId          = MAD_CHASER_ALERT_RELEASE_ANIM;
        animationWork->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
        work->subState                 = work->subState + 1;
    }
}

/// Starts the alert's right sidestep after the preceding animation reaches a boundary.
///
/// A boundary, control jump or settled slot 1 starts clip 4 with a four-frame
/// blend at normal rate, marks the move busy and resets its frame counter.
/// Requires live work and a nine-part model; advances the alert sub-state once.
static void _madChaserAlertStartSidestep(Task* task)
{
    enum {
        MAD_CHASER_ALERT_SIDESTEP_ANIM = 4,
    };
    MadChaserWork* work;
    MadChaserWork* animationWork;

    work = task->work;
    if ((_madChaserAnimHasBoundaryStatus(task) << 0x10) != 0) {
        work->busy        = 1;
        work->stateFrames = 0;

        animationWork                  = task->work;
        animationWork->animBlendFrames = 4;
        animationWork->animRate        = ANIMATION_RATE_ONE;
        animationWork->animId          = MAD_CHASER_ALERT_SIDESTEP_ANIM;
        animationWork->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
        work->subState                 = work->subState + 1;
    }
}

/// Moves the root right by 30 parent-coordinate units at normal animation rate.
///
/// Uses the supplied work's heading plus a quarter turn (4096 units per turn)
/// and reloads the task's live work for its rate in sixteenths of a frame.
/// Scaling retains the signed low 20 product bits before division by 16;
/// the displacement then narrows to s16. Requires live work and root coordinates.
static inline void _madChaserAlertMoveRight(Task* task, MadChaserWork* work)
{
    enum { MAD_CHASER_ALERT_RIGHT_DISTANCE = 30 };
    s16 sideHeading;
    s16 stepDistance;

    stepDistance                          = _madChaserScaleByAnimRate(task, MAD_CHASER_ALERT_RIGHT_DISTANCE);
    sideHeading                           = work->rotation.vy + ACTOR_TRANSFORM_ANGLE_TURN / 4;
    task->extra.tmd->coords->coord.t[0]  += ((rsin(sideHeading) << 4) * stepDistance) >> 0x10;
    task->extra.tmd->coords->coord.t[2]  += ((rcos(sideHeading) << 4) * stepDistance) >> 0x10;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Steps right during the alert animation, then resumes combat walking.
///
/// Moves on previous counter values 29..41 inclusive, at 30 parent-coordinate
/// units per normal-rate frame. A slot-1 boundary, jump or settled pose clears
/// busy and enters the walk behavior at sub-state zero. Requires live work and
/// model coordinates; the frame counter is post-incremented as an unsigned halfword.
static void _madChaserAlertSidestep(Task* task)
{
    enum {
        MAD_CHASER_ALERT_STEP_START_FRAME = 29,
        MAD_CHASER_ALERT_STEP_FRAME_COUNT = 13,
    };
    MadChaserWork* work = task->work;

    if ((u16)(work->stateFrames++ - MAD_CHASER_ALERT_STEP_START_FRAME) < MAD_CHASER_ALERT_STEP_FRAME_COUNT) {
        _madChaserAlertMoveRight(task, work);
    }
    if (_madChaserAnimHasBoundaryStatus(task)) {
        MadChaserWork* nextWork;

        work->busy = 0;

        nextWork           = task->work;
        nextWork->state    = MAD_CHASER_COMBAT_STATE_WALK;
        nextWork->subState = 0;
    }
}
