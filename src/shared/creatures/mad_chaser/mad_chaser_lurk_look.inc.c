/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Turns and sways the spine during the lurk look hold, engaging battle after 16 qualifying ticks.
///
/// Increments the elapsed halfword before testing it; exceeding the hold selects
/// the rise behavior. The final 48 hold ticks ease the yaw to zero. Earlier ticks
/// track a player within 3500 parent-coordinate units and 960 heading units of
/// forward, or sway about +/-896 yaw units. Angles use 4096 units per turn.
/// Qualifying ticks accumulate across interruptions; they need not be consecutive.
/// Requires initialized Mad Chaser work and refreshed player distance/bearing.
static void _madChaserLurkLookAround(Task* task)
{
    enum {
        MAD_CHASER_LOOK_RELAX_FRAMES   = 48,
        MAD_CHASER_LOOK_ENGAGE_TICKS   = 16,
        MAD_CHASER_LOOK_FORWARD_LIMIT  = 960,
        MAD_CHASER_LOOK_BACKWARD_LIMIT = 3136,
        MAD_CHASER_LOOK_SWAY_YAW       = 896,
    };
    MadChaserWork* work = task->work;
    MadChaserWork* nextWork;
    MadChaserWork* alertWork;
    s32            spineYaw;
    s32            relaxYaw;
    s32            playerBearing;

    if ((s16)++work->stateFrames > work->holdFrames) {
        nextWork           = task->work;
        nextWork->state    = MAD_CHASER_LURK_STATE_RISE;
        nextWork->subState = 0;
        return;
    }
    // Relax the spine before the hold ends and the rise transition begins.
    if (work->holdFrames - MAD_CHASER_LOOK_RELAX_FRAMES < (s16)work->stateFrames) {
        relaxYaw       = (u16)work->spineYaw;
        work->spineYaw = relaxYaw + ((s16)(-(relaxYaw * 16)) >> 9);
        return;
    }
    if (work->playerDist < MAD_CHASER_LURK_ALERT_DISTANCE && (playerBearing = (u16)work->playerBearing, (playerBearing < MAD_CHASER_LOOK_FORWARD_LIMIT || playerBearing > MAD_CHASER_LOOK_BACKWARD_LIMIT))) {
        spineYaw       = (u16)work->spineYaw;
        work->spineYaw = spineYaw + ((s16)((playerBearing - spineYaw) * 16) >> 6);
        if (++work->lookFrames >= MAD_CHASER_LOOK_ENGAGE_TICKS) {
            sceneEngageBattle(1);
            alertWork           = task->work;
            alertWork->state    = MAD_CHASER_LURK_STATE_ALERT;
            alertWork->subState = 0;
        }
    } else {
        // Alternate sway targets every 64 running ticks; keep the halfword wrap.
        if (!(((u16)work->frameCount >> 6) & 1)) {
            work->spineYaw = (u16)work->spineYaw + ((s16)((MAD_CHASER_LOOK_SWAY_YAW * 16) - (u16)work->spineYaw * 16) >> 9);
        } else {
            work->spineYaw = (u16)work->spineYaw + ((s16)(-(MAD_CHASER_LOOK_SWAY_YAW * 16) - (u16)work->spineYaw * 16) >> 9);
        }
    }
}

/// Moves the lurk alert laterally using the live rate in sixteenths of a frame.
///
/// distanceAtNormalRate is signed parent-coordinate units per normal-rate frame;
/// positive values move right along heading plus a quarter turn. The shifts
/// retain the signed low 20 product bits before division by 16 and s16 narrowing.
/// Requires the task's live work and model root.
static inline void _madChaserLurkAlertMoveRight(Task* task, const MadChaserWork* work, s32 distanceAtNormalRate)
{
    enum { MAD_CHASER_LURK_ALERT_RATE_FRACTION_BITS = 4 };
    const MadChaserWork* rateWork;
    s16                  sideHeading;
    s16                  stepDistance;

    sideHeading                           = work->rotation.vy + ACTOR_TRANSFORM_ANGLE_TURN / 4;
    rateWork                              = task->work;
    stepDistance                          = (rateWork->animRate * distanceAtNormalRate) << (16 - MAD_CHASER_LURK_ALERT_RATE_FRACTION_BITS) >> 16;
    task->extra.tmd->coords->coord.t[0]  += ((rsin(sideHeading) << 4) * stepDistance) >> 16;
    task->extra.tmd->coords->coord.t[2]  += ((rcos(sideHeading) << 4) * stepDistance) >> 16;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Finishes the lurk alert's right step and enters combat walking.
///
/// Moves on previous counter values 29..41 inclusive by 30 parent-coordinate
/// units per normal-rate frame, scaled by the live animation rate. Post-increments
/// the u16 counter. A slot-1 boundary, jump or held pose clears busy, enters the
/// combat task state, selects walk behavior and resets its sub-state.
/// Requires initialized work and live root coordinates.
static void _madChaserLurkSidestepToCombat(Task* task)
{
    enum {
        MAD_CHASER_LURK_ALERT_STEP_START_FRAME = 29,
        MAD_CHASER_LURK_ALERT_STEP_FRAME_COUNT = 13,
        MAD_CHASER_LURK_ALERT_STEP_DISTANCE    = 30,
    };
    MadChaserWork* work;

    work = task->work;
    if ((u16)(work->stateFrames++ - MAD_CHASER_LURK_ALERT_STEP_START_FRAME) < MAD_CHASER_LURK_ALERT_STEP_FRAME_COUNT) {
        _madChaserLurkAlertMoveRight(task, work, MAD_CHASER_LURK_ALERT_STEP_DISTANCE);
    }
    if (_madChaserAnimHasBoundaryStatusInline(task)) {
        work->busy = 0;
        _madChaserEnterTaskState(task, MAD_CHASER_TASK_COMBAT);
        _madChaserSetBehaviorState(task, MAD_CHASER_COMBAT_STATE_WALK);
    }
}

/// Applies the lurk right step along the heading's positive lateral axis.
///
/// distanceAtNormalRate is signed parent-coordinate units per normal-rate frame;
/// a positive value moves right. Uses the supplied heading plus a quarter turn
/// (4096 units per turn), with the task's live animation rate in sixteenths.
/// Scaling retains the signed low 20 product bits before division by 16 and
/// narrows the result to s16. Requires live work and root coordinates.
static inline void _madChaserLurkMoveRight(Task* task, MadChaserWork* work, s32 distanceAtNormalRate)
{
    enum { MAD_CHASER_LURK_RIGHT_RATE_FRACTION_BITS = 4 };
    MadChaserWork* rateWork;
    s16            sideHeading;
    s16            stepDistance;

    sideHeading                           = work->rotation.vy + ACTOR_TRANSFORM_ANGLE_TURN / 4;
    rateWork                              = task->work;
    stepDistance                          = (rateWork->animRate * distanceAtNormalRate) << (16 - MAD_CHASER_LURK_RIGHT_RATE_FRACTION_BITS) >> 16;
    task->extra.tmd->coords->coord.t[0]  += ((rsin(sideHeading) << 4) * stepDistance) >> 0x10;
    task->extra.tmd->coords->coord.t[2]  += ((rcos(sideHeading) << 4) * stepDistance) >> 0x10;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Steps right in the lurk shift and starts its return animation.
///
/// Moves on previous counter values 29..41 inclusive, at 30 parent-coordinate
/// units per normal-rate frame along heading plus a quarter turn. A slot-1
/// boundary, jump or settled pose clears busy, starts clip 3 with an eight-frame
/// blend at normal rate, resets stateFrames and advances to the left step.
/// Requires live work and root coordinates; this step does not join shared alerts.
static void _madChaserLurkSidestepRight(Task* task)
{
    enum {
        MAD_CHASER_LURK_RIGHT_START_FRAME = 29,
        MAD_CHASER_LURK_RIGHT_FRAME_COUNT = 13,
        MAD_CHASER_LURK_SHIFT_LEFT_ANIM   = 3,
    };
    MadChaserWork* work;
    MadChaserWork* animationWork;
    s32            animationBoundary;

    work = task->work;
    if ((u16)(work->stateFrames++ - MAD_CHASER_LURK_RIGHT_START_FRAME) < MAD_CHASER_LURK_RIGHT_FRAME_COUNT) {
        _madChaserLurkMoveRight(task, work, 30);
    }

    animationWork = task->work;
    if ((animationWork->slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (animationWork->slots[1].status.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        animationBoundary = 1;
    } else {
        animationBoundary = 0;
    }
    if (animationBoundary) {
        work->busy = 0;

        animationWork                  = task->work;
        animationWork->animBlendFrames = 8;
        animationWork->animRate        = ANIMATION_RATE_ONE;
        animationWork->animId          = MAD_CHASER_LURK_SHIFT_LEFT_ANIM;
        animationWork->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
        work->stateFrames              = 0;
        work->subState++;
    }
}

/// Applies the lurk left step along the heading's positive lateral axis.
///
/// distanceAtNormalRate is signed parent-coordinate units per normal-rate frame;
/// a negative value moves left. Uses the supplied heading plus a quarter turn
/// (4096 units per turn), with the task's live animation rate in sixteenths.
/// Scaling retains the signed low 20 product bits before division by 16 and
/// narrows the result to s16. Requires live work and root coordinates.
static inline void _madChaserLurkMoveLeft(Task* task, MadChaserWork* work, s32 distanceAtNormalRate)
{
    enum { MAD_CHASER_LURK_LEFT_RATE_FRACTION_BITS = 4 };
    MadChaserWork* rateWork;
    s16            sideHeading;
    s16            stepDistance;

    sideHeading                           = work->rotation.vy + ACTOR_TRANSFORM_ANGLE_TURN / 4;
    rateWork                              = task->work;
    stepDistance                          = (rateWork->animRate * distanceAtNormalRate) << (16 - MAD_CHASER_LURK_LEFT_RATE_FRACTION_BITS) >> 16;
    task->extra.tmd->coords->coord.t[0]  += ((rsin(sideHeading) << 4) * stepDistance) >> 0x10;
    task->extra.tmd->coords->coord.t[2]  += ((rcos(sideHeading) << 4) * stepDistance) >> 0x10;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Steps left to finish the lurk shift and returns to lurk idle.
///
/// A claimed shared alert takes precedence. Otherwise moves on previous counter
/// values 23..35 inclusive, at -30 parent-coordinate units per normal-rate frame
/// along heading plus a quarter turn. A slot-1 boundary, jump or settled pose
/// clears busy and resets the lurk behavior and sub-state to zero.
/// Requires live work and root coordinates; the frame counter wraps as u16.
static void _madChaserLurkSidestepLeft(Task* task)
{
    enum {
        MAD_CHASER_LURK_LEFT_START_FRAME = 23,
        MAD_CHASER_LURK_LEFT_FRAME_COUNT = 13,
    };
    MadChaserWork* work;
    MadChaserWork* animationWork;
    MadChaserWork* nextWork;
    s32            animationBoundary;

    work = task->work;
    if ((_madChaserJoinAlert(task) << 0x10) == 0) {
        if ((u16)(work->stateFrames++ - MAD_CHASER_LURK_LEFT_START_FRAME) < MAD_CHASER_LURK_LEFT_FRAME_COUNT) {
            _madChaserLurkMoveLeft(task, work, -30);
        }

        animationWork = task->work;
        if ((animationWork->slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) ||
            (animationWork->slots[1].status.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
            animationBoundary = 1;
        } else {
            animationBoundary = 0;
        }
        if (animationBoundary) {
            work->busy = 0;

            nextWork           = task->work;
            nextWork->state    = MAD_CHASER_LURK_STATE_IDLE;
            nextWork->subState = 0;
        }
    }
}

#include "mad_chaser_frame_shadows.inc.c"

/// Advances room-entry behavior, animation and contacts, then refreshes presentation.
///
/// Requires the extended task table, live enemy/model/work, initialized nine-part
/// animation and behavior state 0..9. Running frames consume a vanish command
/// before dispatch, then tick animation, rebuild rotation and apply contacts.
/// A consumed command skips only the old behavior. Paused frames still update
/// color and draw ground shadows when shadowHidden is zero; hidden frames
/// suppress model drawing and return. Borrows task-owned storage and requires
/// initialized rendering scratch and frame-arena space.
static void _madChaserEmergeTick(Task* task)
{
    TmdObject*      model     = task->extra.tmd;
    MadChaserWork*  work      = task->work;
    GfxCoord*       rootCoord = model->coords;
    TaskFuncTable10 states    = _gMadChaserEmergeStates;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->frameCount++;
            if (_madChaserTakeVanishCommand(task) == 0) {
                states.funcs[(s16)work->state](task);
            }
            _madChaserTickAnim(task);
            _madChaserUpdateRotation(task);
            _madChaserApplyContacts(task, 0);
            rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            // Fall through so running and paused frames share presentation.
        case SCENE_COMBAT_ACTORS_PAUSED:
            _madChaserUpdateColor(task->spawnArg2.pointer, &task->extra.tmd->coords[1]);
            if (work->shadowHidden == 0) {
                _madChaserDrawFrameShadows(task);
            }
            return;
    }
}
