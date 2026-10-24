#include "main/random.h"

/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Starts a lurk idle hold with a random 96..159-tick threshold.
///
/// Requests the idle clip with a four-frame blend at normal rate and resets the
/// elapsed counter, then advances to the wait sub-state. Its elapsed test is
/// strict and precedes the increment. Advances the random sequence once; requires initialized
/// Mad Chaser work.
static void _madChaserLurkStartIdleHold(Task* task)
{
    enum {
        MAD_CHASER_IDLE_HOLD_CLIP         = 1,
        MAD_CHASER_IDLE_HOLD_BLEND_FRAMES = 4,
        MAD_CHASER_IDLE_HOLD_MIN_FRAMES   = 96,
        MAD_CHASER_IDLE_HOLD_RANDOM_MASK  = 63,
    };
    MadChaserWork* work;

    work                  = task->work;
    work->animBlendFrames = MAD_CHASER_IDLE_HOLD_BLEND_FRAMES;
    work->animRate        = ANIMATION_RATE_ONE;
    work->animId          = MAD_CHASER_IDLE_HOLD_CLIP;
    work->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
    RANDOM_LCG_ADVANCE();
    work->holdFrames  = (RANDOM_LCG_DRAW() & MAD_CHASER_IDLE_HOLD_RANDOM_MASK) + MAD_CHASER_IDLE_HOLD_MIN_FRAMES;
    work->stateFrames = 0;
    work->subState    = work->subState + 1;
}

#include "main/random.h"

/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Waits through the lurk idle hold, interrupting it when the nearer player comes close.
///
/// Expires when the old elapsed signed halfword exceeds holdFrames, before its
/// post-increment. Expiration selects
/// look or shift with equal random probability. Before expiration, distance below
/// 3500 parent-coordinate units starts alert; below 5000 advances to the idle-end
/// sub-state. Requires initialized Mad Chaser work and refreshed player distance.
static void _madChaserLurkWait(Task* task)
{
    MadChaserWork* work = task->work;
    s16            playerDistance;

    if (work->holdFrames < (s16)work->stateFrames++) {
        RANDOM_LCG_ADVANCE();
        if (RANDOM_LCG_DRAW() & 1) {
            MadChaserWork* nextWork = task->work;

            nextWork->state    = MAD_CHASER_LURK_STATE_SHIFT;
            nextWork->subState = 0;
        } else {
            MadChaserWork* nextWork = task->work;

            nextWork->state    = MAD_CHASER_LURK_STATE_LOOK;
            nextWork->subState = 0;
        }
        return;
    }
    playerDistance = work->playerDist;
    if (playerDistance < MAD_CHASER_LURK_ALERT_DISTANCE) {
        MadChaserWork* alertWork = task->work;

        alertWork->state    = MAD_CHASER_LURK_STATE_ALERT;
        alertWork->subState = 0;
        return;
    }
    if (playerDistance < MAD_CHASER_LURK_LOOK_DISTANCE) {
        work->subState++;
    }
}

/// Starts the lurk look behavior once the idle clip reports a boundary or jump.
///
/// Selects the look behavior at sub-state zero; the frame counter is retained
/// until the look hold begins. Requires the initialized Mad Chaser work.
static void _madChaserLurkIdleEnd(Task* task)
{
    MadChaserWork* work;
    s32            hasBoundaryStatus;

    work = task->work;
    if ((work->slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (work->slots[1].status.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        hasBoundaryStatus = 1;
    } else {
        hasBoundaryStatus = 0;
    }
    if (hasBoundaryStatus) {
        work           = task->work;
        work->state    = MAD_CHASER_LURK_STATE_LOOK;
        work->subState = 0;
    }
}

/// Requests the transition into the lurk look sequence once slot 1 reaches a control point.
///
/// Blends the transition clip over eight frames at normal rate, then advances
/// the look sequence's sub-state. Requires the initialized Mad Chaser work.
static void _madChaserLurkPrepareLook(Task* task)
{
    enum {
        MAD_CHASER_LOOK_TRANSITION_CLIP         = 13,
        MAD_CHASER_LOOK_TRANSITION_BLEND_FRAMES = 8,
    };
    MadChaserWork* work;
    MadChaserWork* requestWork;
    s32            hasBoundaryStatus;

    work = task->work;
    if ((work->slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (work->slots[1].status.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        hasBoundaryStatus = 1;
    } else {
        hasBoundaryStatus = 0;
    }
    if (hasBoundaryStatus) {
        requestWork                  = task->work;
        requestWork->animBlendFrames = MAD_CHASER_LOOK_TRANSITION_BLEND_FRAMES;
        requestWork->animRate        = ANIMATION_RATE_ONE;
        requestWork->animId          = MAD_CHASER_LOOK_TRANSITION_CLIP;
        requestWork->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
        work->subState++;
    }
}

#include "main/random.h"

/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Starts a look hold with a random 176..239-tick threshold at an animation control point.
///
/// Requests the look clip with a four-frame blend at normal rate, resets elapsed
/// and qualifying-look counters, and advances to the look update sub-state.
/// Advances the shared random sequence once. Requires initialized Mad Chaser work.
static void _madChaserLurkStartLookHold(Task* task)
{
    enum {
        MAD_CHASER_LOOK_HOLD_CLIP         = 14,
        MAD_CHASER_LOOK_HOLD_BLEND_FRAMES = 4,
        MAD_CHASER_LOOK_HOLD_MIN_FRAMES   = 176,
        MAD_CHASER_LOOK_HOLD_RANDOM_MASK  = 63,
    };
    MadChaserWork* work;
    MadChaserWork* requestWork;
    s32            hasBoundaryStatus;

    work = task->work;
    if ((work->slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (work->slots[1].status.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        hasBoundaryStatus = 1;
    } else {
        hasBoundaryStatus = 0;
    }
    if (hasBoundaryStatus) {
        requestWork                  = task->work;
        requestWork->animBlendFrames = MAD_CHASER_LOOK_HOLD_BLEND_FRAMES;
        requestWork->animRate        = ANIMATION_RATE_ONE;
        requestWork->animId          = MAD_CHASER_LOOK_HOLD_CLIP;
        requestWork->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
        RANDOM_LCG_ADVANCE();
        work->stateFrames = 0;
        work->lookFrames  = 0;
        work->holdFrames  = (RANDOM_LCG_DRAW() & MAD_CHASER_LOOK_HOLD_RANDOM_MASK) + MAD_CHASER_LOOK_HOLD_MIN_FRAMES;
        work->subState++;
    }
}

/// Starts the lurk rise with a four-frame blend into clip 15.
///
/// Requires initialized work/animation storage and the rise entry sub-state.
/// Requests normal-rate playback and advances to its boundary wait; the frame
/// callback applies the request and ticks the slots.
static void _madChaserLurkRiseStart(Task* task)
{
    enum {
        MAD_CHASER_LURK_RISE_CLIP         = 15,
        MAD_CHASER_LURK_RISE_BLEND_FRAMES = 4,
    };
    MadChaserWork* work = task->work;

    work->animBlendFrames = MAD_CHASER_LURK_RISE_BLEND_FRAMES;
    work->animRate        = ANIMATION_RATE_ONE;
    work->animId          = MAD_CHASER_LURK_RISE_CLIP;
    work->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
    work->subState        = work->subState + 1;
}

/// Returns the lurk rise to idle when slot 1 reaches an animation boundary.
///
/// Requires initialized animation/work storage. A boundary, control jump or
/// held pose selects lurk idle at sub-state zero; other statuses leave the
/// behavior intact. Does not consume status or tick the animation.
static void _madChaserLurkRiseEnd(Task* task)
{
    if (_madChaserAnimHasBoundaryStatusInline(task)) {
        _madChaserSetBehaviorState(task, MAD_CHASER_LURK_STATE_IDLE);
    }
}

/// Begins the lurk alert animation and engages an idle scene battle.
///
/// Blends to clip 15 over eight normal-rate frames, advances the sub-state and
/// retains the frame counter and busy flag. Requires initialized Mad Chaser
/// work. Engaging battle does not acquire a reference or claim the shared alert.
static void _madChaserStartAlert(Task* task)
{
    enum {
        MAD_CHASER_LURK_ALERT_CLIP         = 15,
        MAD_CHASER_LURK_ALERT_BLEND_FRAMES = 8,
    };
    MadChaserWork* work = task->work;

    work->animBlendFrames = MAD_CHASER_LURK_ALERT_BLEND_FRAMES;
    work->animRate        = ANIMATION_RATE_ONE;
    work->animId          = MAD_CHASER_LURK_ALERT_CLIP;
    work->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
    work->subState        = work->subState + 1;
    sceneEngageBattle(1);
}

/// Starts the lurk alert's right sidestep after the preceding animation boundary.
///
/// A slot-1 boundary, jump or held pose resets the frame counter, marks the
/// move busy, blends to clip 4 over four normal-rate frames and advances the
/// sub-state. Requires initialized work; task and behavior state are retained.
static void _madChaserLurkAlertStartSidestep(Task* task)
{
    enum {
        MAD_CHASER_LURK_ALERT_SIDESTEP_CLIP         = 4,
        MAD_CHASER_LURK_ALERT_SIDESTEP_BLEND_FRAMES = 4,
    };
    MadChaserWork* work;
    MadChaserWork* requestWork;

    work = task->work;
    if (_madChaserAnimHasBoundaryStatusInline(task)) {
        work->stateFrames            = 0;
        work->busy                   = 1;
        requestWork                  = task->work;
        requestWork->animBlendFrames = MAD_CHASER_LURK_ALERT_SIDESTEP_BLEND_FRAMES;
        requestWork->animRate        = ANIMATION_RATE_ONE;
        requestWork->animId          = MAD_CHASER_LURK_ALERT_SIDESTEP_CLIP;
        requestWork->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
        work->subState++;
    }
}

/// Starts the lurk shift's opening animation unless a shared alert takes over.
///
/// A claimed alert switches the task to combat. Otherwise starts clip 15 with
/// an eight-normal-frame blend and advances to the next shift sub-state.
/// Requires live work; busy and the frame counter are retained.
static void _madChaserLurkShiftStart(Task* task)
{
    enum {
        MAD_CHASER_LURK_SHIFT_START_ANIM = 15,
    };
    MadChaserWork* work;
    MadChaserWork* animationWork;

    work = task->work;
    if (_madChaserJoinAlert(task) == 0) {

        animationWork                  = task->work;
        animationWork->animBlendFrames = 8;
        animationWork->animRate        = ANIMATION_RATE_ONE;
        animationWork->animId          = MAD_CHASER_LURK_SHIFT_START_ANIM;
        animationWork->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
        work->subState                 = work->subState + 1;
    }
}

/// Starts the lurk shift's right sidestep at the preceding animation boundary.
///
/// A claimed shared alert takes precedence and switches to combat. Otherwise a
/// slot-1 boundary, control jump or settled pose starts clip 4 with a four-frame
/// blend at normal rate, sets busy, resets stateFrames and advances the sub-state.
/// Requires live Mad Chaser work and loaded animation data.
static void _madChaserLurkShiftStartSidestep(Task* task)
{
    enum {
        MAD_CHASER_LURK_SHIFT_RIGHT_ANIM = 4,
    };
    MadChaserWork* work;
    MadChaserWork* statusWork;
    MadChaserWork* animationWork;
    s32            animationBoundary;

    work = task->work;
    if ((_madChaserJoinAlert(task) << 0x10) == 0) {

        statusWork = task->work;
        if ((statusWork->slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) ||
            (statusWork->slots[1].status.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
            animationBoundary = 1;
        } else {
            animationBoundary = 0;
        }
        if (animationBoundary) {
            work->stateFrames = 0;
            work->busy        = 1;

            animationWork                  = task->work;
            animationWork->animBlendFrames = 4;
            animationWork->animRate        = ANIMATION_RATE_ONE;
            animationWork->animId          = MAD_CHASER_LURK_SHIFT_RIGHT_ANIM;
            animationWork->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
            work->subState                 = work->subState + 1;
        }
    }
}

/// Begins a scripted pull toward joint 3 of the room's placed actor 0.
///
/// Requires live Mad Chaser work/model and that placed actor's coordinate chain.
/// Stops the global alert cry while retaining release, saves the root's local
/// XYZ narrowed to s16, clears frame/motion counters and advances the sub-state.
/// Transforms the joint origin up to the excluded view node, narrowing XYZ at
/// every parent. A parentless chain leaves pullPoint zero. Both actors must use
/// the world frame under the view as their root parent; GTE state is overwritten.
static void _madChaserPullStart(Task* task)
{
    enum { MAD_CHASER_PULL_POINT_PLACED_ACTOR = 0,
           MAD_CHASER_PULL_POINT_JOINT        = 3 };
    MadChaserWork* work;
    GfxCoord*      root;
    SVECTOR*       pullPoint;

    work = task->work;
    root = task->extra.tmd->coords;
    sndEvtRequestScriptStop(SOUND_MAD_CHASER_ALERT_CRY, SOUND_SCRIPT_STOP_KEEP_RELEASE);
    work->moveStartPos.vx = root->coord.t[0];
    work->moveStartPos.vy = root->coord.t[1];
    work->moveStartPos.vz = root->coord.t[2];
    work->stateFrames     = 0;
    work->moveAccel       = 0;
    work->moveSpeed       = 0;
    work->subState++;
    // An incomplete parent chain retains the zero pull point.
    pullPoint     = &work->pullPoint;
    pullPoint->vx = pullPoint->vy = pullPoint->vz = 0;
    _actorRenderTransformLocalPointToWorld(&sceneFindPlacedActor(MAD_CHASER_PULL_POINT_PLACED_ACTOR)->extra.tmd->coords[MAD_CHASER_PULL_POINT_JOINT], pullPoint);
}

/// Selects the limp or resisting pull sequence from the current animation stance.
///
/// Requires initialized work, a live enemy/model and an animId with a valid
/// stance-table entry. Low stance blends clip 9 over four normal-rate frames,
/// plays the positional alert cry and selects limp drag (sub-state 4). Upright
/// stance blends clip 7 over eight frames, seeds the four-counts-per-rate-unit
/// ramp and advances to struggle. Animation playback belongs to the caller.
static void _madChaserPullReact(Task* task)
{
    enum {
        MAD_CHASER_PULL_LIMP_CLIP                 = 9,
        MAD_CHASER_PULL_LIMP_BLEND_FRAMES         = 4,
        MAD_CHASER_PULL_STRUGGLE_CLIP             = 7,
        MAD_CHASER_PULL_STRUGGLE_BLEND_FRAMES     = 8,
        MAD_CHASER_PULL_LIMP_SUB_STATE            = 4,
        MAD_CHASER_PULL_RATE_RAMP_COUNTS_PER_UNIT = 4,
    };
    MadChaserWork* work;
    s32            soundId;
    s32            audioPan;

    work = task->work;
    if (_gMadChaserAnimStance[work->animId - 1] == MAD_CHASER_STANCE_LOW) {
        work->animBlendFrames = MAD_CHASER_PULL_LIMP_BLEND_FRAMES;
        work->animRate        = ANIMATION_RATE_ONE;
        work->animId          = MAD_CHASER_PULL_LIMP_CLIP;
        work->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
        soundId               = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_MAD_CHASER_ALERT_CRY;
        audioPan              = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        work->subState = MAD_CHASER_PULL_LIMP_SUB_STATE;
        return;
    }
    work->animBlendFrames = MAD_CHASER_PULL_STRUGGLE_BLEND_FRAMES;
    work->animRate        = ANIMATION_RATE_ONE;
    work->animId          = MAD_CHASER_PULL_STRUGGLE_CLIP;
    work->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
    work->stateScratch    = (u8)work->animRate * MAD_CHASER_PULL_RATE_RAMP_COUNTS_PER_UNIT;
    work->subState++;
}

/// Detaches and hides the enemy before the timed vanish teardown.
///
/// Stops the alert cry with its release retained and clears the shared alert
/// owner only when this enemy owns it. Unlinks targeting and all three collision
/// bodies, detaches the enemy's borrowed contact records and hides active drawing.
/// Resets the frame counter and advances the behavior. Requires live work, model
/// and enemy; their allocations and the scene battle reference remain live.
static void _madChaserVanish(Task* task)
{
    MadChaserWork* collisionWork;
    MadChaserWork* work;
    Enemy*         enemy;
    TmdObject*     model;

    work              = task->work;
    enemy             = task->spawnArg2.pointer;
    model             = task->extra.tmd;
    work->stateFrames = 0;
    sndEvtRequestScriptStop(SOUND_MAD_CHASER_ALERT_CRY, SOUND_SCRIPT_STOP_KEEP_RELEASE);
    if ((gSceneCombatState.madChaserAlertOwner & SCENE_COMBAT_MAD_CHASER_OWNER_MASK) == (((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT)) {
        gSceneCombatState.madChaserAlertOwner = 0;
    }
    worldTargetUnlinkNode(&enemy->node);
    // Detach every list user before the later state releases storage.
    enemy->recs   = NULL;
    collisionWork = task->work;
    worldCollisionUnlinkBody(&collisionWork->pairBody);
    worldCollisionUnlinkBody(&collisionWork->gridBody);
    worldCollisionUnlinkBody(&collisionWork->attackBody);
    model->flags = model->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW;
    work->state  = work->state + 1;
}

/// Releases the hidden model's primitive buffer, then destroys the vanished enemy.
///
/// Requires the detached task left by `_madChaserVanish`. Increments its u16
/// frame counter, testing it as s16: frame 3 frees the primitive buffer and
/// disables automatic buffer allocation; frame 36 or later destroys the enemy
/// and begins task teardown. No task or enemy access is valid after destruction.
static void _madChaserVanishFree(Task* task)
{
    enum {
        MAD_CHASER_VANISH_RELEASE_BUFFER_FRAME = 3,
        MAD_CHASER_VANISH_DESTROY_FRAME        = 36,
    };
    MadChaserWork* work;
    TmdObject*     model;
    u16            elapsedFrames;

    work              = task->work;
    model             = task->extra.tmd;
    elapsedFrames     = work->stateFrames + 1;
    work->stateFrames = elapsedFrames;
    if ((s16)elapsedFrames == MAD_CHASER_VANISH_RELEASE_BUFFER_FRAME) {
        tmdFreePrimitiveBuffer(model);
        model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    if ((s16)work->stateFrames >= MAD_CHASER_VANISH_DESTROY_FRAME) {
        enemyDestroy(task->spawnArg2.pointer, task);
    }
}
