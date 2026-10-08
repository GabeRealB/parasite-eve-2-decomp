/* Part of the Odd Stranger library; see odd_stranger.h. */

/// Turns through twice the player bearing, then starts another dash or a grab.
///
/// Handles `ODD_STRANGER_STATE_TURN_AROUND` on a live Odd Stranger task with
/// bound rigs and a live player in the same root-parent space. Entry stores
/// the current root heading and a signed-halfword target heading offset by
/// twice the wrapped player turn. Later ticks approach that target by 137
/// angle units (4096 per turn), with no further wrapping. A tick that starts
/// at the target selects `GRAB` after two dashes, inside 900 coordinate units
/// and with no cooldown (variant 2 also requires sight), otherwise `CIRCLE_DASH`.
/// Forward steps are 40 units, or 20 during blending; the cooldown counts
/// down on subsequent ticks. Borrows one chase scratch block plus nested
/// contact, movement and range-test workspace.
static void _oddStrangerTurnAround(Task* task)
{
    enum {
        ODD_STRANGER_TURN_AROUND_GRAB_DASH_COUNT = 2,
        ODD_STRANGER_TURN_AROUND_GRAB_RADIUS     = 900,
        ODD_STRANGER_TURN_AROUND_STEP            = 137,
        ODD_STRANGER_TURN_AROUND_FORWARD_STEP    = 40,
        ODD_STRANGER_TURN_AROUND_BLEND_STEP      = 20
    };
    OddStrangerWork*   work;
    ActorChaseScratch* turnScratch;
    ActorChaseScratch* savedCursor;
    TmdObject*         model;
    GfxCoord*          headingRoot;
    GfxCoord*          facingRoot;

    work = task->work;
    if (work->stateEntered != 0) {
        savedCursor                                               = SCRATCH_STACK_CURSOR(ActorChaseScratch);
        model                                                     = task->extra.tmd;
        SCRATCH_STACK_CURSOR(ActorChaseScratch)                   = savedCursor - 1;
        turnScratch                                               = savedCursor - 1;
        ((Enemy*)task->spawnArg2.pointer)->node.state.parts.flags = 0;
        model->flags                                              = 0;
        tmdAllocPrimitiveBuffer(model);
        work->hitBody.radius    = ODD_STRANGER_BODY_RADIUS;
        work->animRequest       = ODD_STRANGER_ANIM_REQUEST_BLEND;
        work->animRate          = ANIMATION_RATE_ONE;
        work->animId            = ODD_STRANGER_ANIM_RUN;
        work->blendActive       = 0;
        work->lookYawTarget     = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _oddStrangerDriveAnimation(task);
        _actorPositionDeltaToPlayer(&gPlayerStatus, task->extra.tmd->coords, &turnScratch->delta);
        headingRoot          = task->extra.tmd->coords;
        turnScratch->turn    = _actorAngleNormalizeYaw(ratan2(turnScratch->delta.vx, turnScratch->delta.vz) - ratan2(-headingRoot->coord.m[2][0], headingRoot->coord.m[2][2]));
        facingRoot           = task->extra.tmd->coords;
        turnScratch->heading = ratan2(-facingRoot->coord.m[2][0], facingRoot->coord.m[2][2]);
        // Keep the doubled turn as a signed-halfword target without wrapping it.
        work->turnYaw       = turnScratch->heading;
        work->turnYawTarget = turnScratch->heading + (u16)turnScratch->turn * 2;
        SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
        return;
    }
    savedCursor                             = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    SCRATCH_STACK_CURSOR(ActorChaseScratch) = savedCursor - 1;
    turnScratch                             = savedCursor - 1;
    _oddStrangerDriveAnimation(task);
    _actorPositionDeltaToPlayer(&gPlayerStatus, task->extra.tmd->coords, &turnScratch->delta);
    if (work->turnYaw == work->turnYawTarget) {
        if (work->dashCount < ODD_STRANGER_TURN_AROUND_GRAB_DASH_COUNT || _oddStrangerOutOfRange(&turnScratch->delta, ODD_STRANGER_TURN_AROUND_GRAB_RADIUS)
#if ODD_STRANGER_SIGHT_TEST
            || _playerDetectionSightBlocked(task) == 1
#endif
            || work->grabCooldown != 0) {
            work->state = ODD_STRANGER_STATE_CIRCLE_DASH;
        } else {
            work->state = ODD_STRANGER_STATE_GRAB;
        }
    }
    if (work->turnYaw > work->turnYawTarget) {
        work->turnYaw -= ODD_STRANGER_TURN_AROUND_STEP;
        if (work->turnYaw < work->turnYawTarget) {
            work->turnYaw = work->turnYawTarget;
        }
    }
    if (work->turnYaw < work->turnYawTarget) {
        work->turnYaw += ODD_STRANGER_TURN_AROUND_STEP;
        if (work->turnYaw > work->turnYawTarget) {
            work->turnYaw = work->turnYawTarget;
        }
    }
    gfxRotMatrixY(&task->extra.tmd->coords->coord, work->turnYaw, GRAPHICS_ROTATION_REPLACE);
    _actorRenderRescaleYaw(task->extra.tmd->coords, ODD_STRANGER_ROOT_SCALE);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->blendActive == 0) {
        if ((s16)_playerDetectionOutOfReach(task->extra.tmd->coords, ODD_STRANGER_MOVE_STOP_DISTANCE, ODD_STRANGER_TURN_AROUND_FORWARD_STEP) != 0) {
            _actorMovementStepForward(task->extra.tmd->coords, ODD_STRANGER_TURN_AROUND_FORWARD_STEP);
        }
    } else {
        if ((s16)_playerDetectionOutOfReach(task->extra.tmd->coords, ODD_STRANGER_MOVE_STOP_DISTANCE, ODD_STRANGER_TURN_AROUND_BLEND_STEP) != 0) {
            _actorMovementStepForward(task->extra.tmd->coords, ODD_STRANGER_TURN_AROUND_BLEND_STEP);
        }
    }
    if (_actorContactApplyGridPushback(task->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts)) != 1) {
        _oddStrangerApplyBodyPushback(task, work->hitContacts, ARRAY_SIZE(work->hitContacts));
    }
    if (work->grabCooldown != 0) {
        work->grabCooldown--;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}
