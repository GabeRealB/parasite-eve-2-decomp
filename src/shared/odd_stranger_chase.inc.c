/* Part of the Odd Stranger library; see odd_stranger.h. */

/// Snapshots the player heading and the reverse bearing used by the circling dash.
///
/// Requires live enemy/player roots in the same parent frame and reserved
/// scratch. The player heading comes from its local matrix, while the reverse
/// bearing comes from the signed-halfword X/Z offset; angles use 4096 units
/// per turn and the reverse bearing wraps to [-2048, 2048]. No root is composed
/// or rotated. Scratch belongs to the caller and remains reserved.
/// Nonzero `refreshOffset` reads a fresh player offset after the heading;
/// zero uses the caller's existing `chase->delta`.
static __inline__ void _oddStrangerReadCircleDashPlayerBearings(const Task* task, ActorChaseScratch* chase, s32 refreshOffset)
{
    chase->playerYaw = ratan2(-(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][0],
                              (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][2]);
    if (refreshOffset) {
        _actorPositionDeltaToPlayer(&gPlayerStatus, task->extra.tmd->coords, &chase->delta);
    }
    chase->yawFromPlayer = ratan2(chase->delta.vx, chase->delta.vz) + ACTOR_TRANSFORM_ANGLE_HALF_TURN;
    chase->yawFromPlayer = _actorAngleNormalizeYaw(chase->yawFromPlayer);
}

/// Dashes around the player, then grabs from behind or coasts into a slide.
///
/// Handles `ODD_STRANGER_STATE_CIRCLE_DASH` on a live Odd Stranger task with
/// bound rigs and a live player in the same root-parent space. Its turn aims
/// 1000 angle units off the player bearing, limited to 96 units per tick;
/// angles use 4096 units per turn. Speed is eight times the playback rate,
/// halved during blending and reduced to two coordinate units after grid
/// pushback. Seven pushes request `SLIDE`; the five-tick settled-rate exit
/// window can supersede that request with `GRAB` when the player's raw bearing
/// difference exceeds a quarter turn and the grab cooldown permits it.
/// Borrows one chase scratch block plus nested contact/movement workspace.
static void _oddStrangerCircleDash(Task* task)
{
    enum {
        ODD_STRANGER_CIRCLE_DASH_ORBIT_BIAS      = 1000,
        ODD_STRANGER_CIRCLE_DASH_TURN_LIMIT      = 96,
        ODD_STRANGER_CIRCLE_DASH_BACK_TURN_LIMIT = ACTOR_TRANSFORM_ANGLE_TURN / 4,
        ODD_STRANGER_CIRCLE_DASH_PUSH_LIMIT      = 7,
        ODD_STRANGER_CIRCLE_DASH_STEP_PER_RATE   = 8,
        ODD_STRANGER_CIRCLE_DASH_PUSHED_STEP     = 2,
        ODD_STRANGER_CIRCLE_DASH_RATE_RISE       = 8,
        ODD_STRANGER_CIRCLE_DASH_RATE_PEAK       = 24,
        ODD_STRANGER_CIRCLE_DASH_RATE_FALL       = -1,
        ODD_STRANGER_CIRCLE_DASH_RATE_SETTLED    = 18,
        ODD_STRANGER_CIRCLE_DASH_EXIT_TICKS      = 5,
        ODD_STRANGER_CIRCLE_DASH_REENTER_STATE   = -1 // Forces fresh entry on the next tick
    };
    ActorChaseScratch* chase;
    ActorChaseScratch* savedCursor;
    OddStrangerWork*   work;
    TmdObject*         model;
    GfxCoord*          headingRoot;
    GfxCoord*          facingRoot;
    s32                playerTurn;
    s32                positiveOrbitError;
    s32                negativeOrbitError;
    s32                playerBearingDifference;

    work = task->work;
    if (work->stateEntered != 0) {
        model                                                     = task->extra.tmd;
        ((Enemy*)task->spawnArg2.pointer)->node.state.parts.flags = 0;
        model->flags                                              = 0;
        tmdAllocPrimitiveBuffer(model);
        work->hitBody.radius    = ODD_STRANGER_SWING_RADIUS;
        work->animRequest       = ODD_STRANGER_ANIM_REQUEST_BLEND;
        work->animId            = ODD_STRANGER_ANIM_RUN;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _oddStrangerDriveAnimation(task);
        work->dashRateStep        = ODD_STRANGER_CIRCLE_DASH_RATE_RISE;
        work->stateTimer          = 0;
        work->exitCounter         = 0;
        gOddStrangerChaseDistance = 0;
        work->dashCount++;
        return;
    }
    savedCursor                             = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    SCRATCH_STACK_CURSOR(ActorChaseScratch) = savedCursor - 1;
    chase                                   = savedCursor - 1;
    task->extra.tmd->coords->composeStamp   = GRAPHICS_COORD_DIRTY;
    _oddStrangerDriveAnimation(task);
#if ODD_STRANGER_VARIANT == 1
    if (_actorContactApplyGridPushback(task->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts)) == 1) {
#else
    if (_actorContactApplyGridPushback(task->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts)) != 0) {
#endif
        work->exitCounter++;
    } else {
        _oddStrangerApplyBodyPushback(task, work->hitContacts, ARRAY_SIZE(work->hitContacts));
    }
    _actorPositionDeltaToPlayer(&gPlayerStatus, task->extra.tmd->coords, &chase->delta);
    if (work->exitCounter >= ODD_STRANGER_CIRCLE_DASH_PUSH_LIMIT) {
        _oddStrangerReadCircleDashPlayerBearings(task, chase, 0);
        work->state = ODD_STRANGER_STATE_SLIDE;
    }
    // Aim off the player bearing to run around them, preserving the side bias.
    headingRoot = task->extra.tmd->coords;
    chase->turn = _actorAngleNormalizeYaw(ratan2(chase->delta.vx, chase->delta.vz) - ratan2(-headingRoot->coord.m[2][0], headingRoot->coord.m[2][2]));
    playerTurn  = chase->turn;
    if (playerTurn >= 0) {
        positiveOrbitError = playerTurn - ODD_STRANGER_CIRCLE_DASH_ORBIT_BIAS;
        if (ABS(positiveOrbitError) < ODD_STRANGER_CIRCLE_DASH_TURN_LIMIT) {
            chase->heading = chase->turn - ODD_STRANGER_CIRCLE_DASH_ORBIT_BIAS;
        } else if (positiveOrbitError > 0) {
            chase->heading = ODD_STRANGER_CIRCLE_DASH_TURN_LIMIT;
        } else {
            chase->heading = -ODD_STRANGER_CIRCLE_DASH_TURN_LIMIT;
        }
    } else {
        negativeOrbitError = playerTurn + ODD_STRANGER_CIRCLE_DASH_ORBIT_BIAS;
        if (ABS(negativeOrbitError) < ODD_STRANGER_CIRCLE_DASH_TURN_LIMIT) {
            chase->heading = chase->turn + ODD_STRANGER_CIRCLE_DASH_ORBIT_BIAS;
        } else if (negativeOrbitError > 0) {
            chase->heading = ODD_STRANGER_CIRCLE_DASH_TURN_LIMIT;
        } else {
            chase->heading = -ODD_STRANGER_CIRCLE_DASH_TURN_LIMIT;
        }
    }
    facingRoot      = task->extra.tmd->coords;
    chase->heading += ratan2(-facingRoot->coord.m[2][0], facingRoot->coord.m[2][2]);
    gfxRotMatrixY(&task->extra.tmd->coords->coord, chase->heading, GRAPHICS_ROTATION_REPLACE);
    _actorRenderRescaleYaw(task->extra.tmd->coords, ODD_STRANGER_ROOT_SCALE);
    headingRoot                           = task->extra.tmd->coords;
    work->lookYawTarget                   = _actorAngleNormalizeYaw(ratan2(chase->delta.vx, chase->delta.vz) - ratan2(-headingRoot->coord.m[2][0], headingRoot->coord.m[2][2]));
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    work->slideStep                       = work->animRate * ODD_STRANGER_CIRCLE_DASH_STEP_PER_RATE;
    if (work->blendActive != 0) {
        work->slideStep = work->slideStep >> 1;
    }
    if (work->exitCounter != 0) {
        work->slideStep = ODD_STRANGER_CIRCLE_DASH_PUSHED_STEP;
    }
    if ((_playerDetectionOutOfReach(task->extra.tmd->coords, ODD_STRANGER_MOVE_STOP_DISTANCE, work->slideStep) << 0x10) != 0) {
        _actorMovementTranslateForwardNonzero(task->extra.tmd->coords, (u16)work->slideStep);
    }
    gOddStrangerChaseDistance += (u16)work->slideStep;
    if (work->dashRateStep == ODD_STRANGER_CIRCLE_DASH_RATE_RISE && work->animRate >= ODD_STRANGER_CIRCLE_DASH_RATE_PEAK) {
        work->dashRateStep = ODD_STRANGER_CIRCLE_DASH_RATE_FALL;
    }
    if (work->dashRateStep == ODD_STRANGER_CIRCLE_DASH_RATE_FALL && work->animRate == ODD_STRANGER_CIRCLE_DASH_RATE_SETTLED) {
        work->dashRateStep = 0;
        work->stateTimer   = 0;
    }
    if (work->dashRateStep == 0) {
        if (++work->stateTimer == ODD_STRANGER_CIRCLE_DASH_EXIT_TICKS) {
            _oddStrangerReadCircleDashPlayerBearings(task, chase, 1);
            playerBearingDifference = chase->yawFromPlayer - chase->playerYaw;
            if (playerBearingDifference < 0) {
                playerBearingDifference = -playerBearingDifference;
            }
            if (playerBearingDifference > ODD_STRANGER_CIRCLE_DASH_BACK_TURN_LIMIT
#if ODD_STRANGER_SIGHT_TEST
                && _playerDetectionSightBlocked(task) != 1
#endif
                && work->grabCooldown == 0) {
                work->state = ODD_STRANGER_STATE_GRAB;
            } else {
                work->state     = ODD_STRANGER_STATE_SLIDE;
                work->prevState = ODD_STRANGER_CIRCLE_DASH_REENTER_STATE;
            }
        }
    }
    work->animRate += (u16)work->dashRateStep;
    if (work->grabCooldown != 0) {
        work->grabCooldown--;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}
