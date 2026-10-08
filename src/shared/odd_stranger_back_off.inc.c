/* Part of the Odd Stranger library; see odd_stranger.h. */

/// Applies the retreat's asymmetric turn and restores the normal model scale.
///
/// Requires a live model root and reserved chase scratch holding the signed
/// player turn. `turnLimit` is positive in 4096ths of a turn (128 at the call):
/// values above it are capped then halved; values below its negative retain
/// the full negative cap; all other values are arithmetically halved.
/// Replaces scratch turn with the resulting absolute heading. Discards pitch
/// and roll, retains translation and marks composition dirty. Nested matrix
/// scratch is released here; the caller retains the chase block.
static __inline__ void _oddStrangerTurnBackOffRoot(Task* task, ActorChaseScratch* retreat, s16 turnLimit)
{
    if (retreat->turn >= (turnLimit + 1)) {
        retreat->turn = turnLimit;
    }
    if (retreat->turn < -turnLimit) {
        retreat->turn = -turnLimit;
    } else {
        retreat->turn = retreat->turn >> 1;
    }
    retreat->turn += ratan2(-task->extra.tmd->coords->coord.m[2][0], task->extra.tmd->coords->coord.m[2][2]);
    gfxRotMatrixY(&task->extra.tmd->coords->coord, retreat->turn, GRAPHICS_ROTATION_REPLACE);
    _actorRenderRescaleYaw(task->extra.tmd->coords, ODD_STRANGER_ROOT_SCALE);
}

/// Faces the player, retreats for 19 ticks, then turns aside and chases.
///
/// Handles the unselected `ODD_STRANGER_STATE_BACK_OFF` on a live Odd Stranger
/// task with bound rigs and a live player in the same root-parent space.
/// The walk changes to the retreat clip once the absolute turn is at most 128
/// angle units. Positive turns are capped and halved; turns below -128 retain
/// the full negative cap. The retreat is 16 coordinate units per tick, followed
/// by a 1200-unit relative yaw turn (4096 per turn).
/// Borrows one chase scratch block plus nested contact/movement workspace.
static void _oddStrangerBackOff(Task* task)
{
    enum {
        ODD_STRANGER_BACK_OFF_RATE       = 22, // Sixteenths of a frame per tick
        ODD_STRANGER_BACK_OFF_ANIM       = 17,
        ODD_STRANGER_BACK_OFF_TURN_LIMIT = 128,
        ODD_STRANGER_BACK_OFF_STEP       = -16,
        ODD_STRANGER_BACK_OFF_TICKS      = 19,
        ODD_STRANGER_BACK_OFF_EXIT_TURN  = 1200
    };
    OddStrangerWork*   work;
    ActorChaseScratch* retreat;
    TmdObject*         model;

    work = task->work;
    if (work->stateEntered != 0) {
        model                                                     = task->extra.tmd;
        ((Enemy*)task->spawnArg2.pointer)->node.state.parts.flags = 0;
        model->flags                                              = 0;
        tmdAllocPrimitiveBuffer(model);
        work->hitBody.radius    = ODD_STRANGER_BODY_RADIUS;
        work->animRequest       = ODD_STRANGER_ANIM_REQUEST_BLEND;
        work->animRate          = ODD_STRANGER_BACK_OFF_RATE;
        work->animId            = ODD_STRANGER_ANIM_WALK;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _oddStrangerDriveAnimation(task);
        return;
    }
    _oddStrangerDriveAnimation(task);
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    retreat             = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    retreat->turn       = _actorAngleTurnToPlayer(task, &retreat->delta, &gPlayerStatus);
    work->lookYawTarget = retreat->turn;
    if (ABS(retreat->turn) < (ODD_STRANGER_BACK_OFF_TURN_LIMIT + 1) && work->animId == ODD_STRANGER_ANIM_WALK) {
        work->animRate    = ODD_STRANGER_BACK_OFF_RATE;
        work->animId      = ODD_STRANGER_BACK_OFF_ANIM;
        work->animRequest = ODD_STRANGER_ANIM_REQUEST_BLEND;
        work->stateTimer  = 0;
        _oddStrangerDriveAnimation(task);
    }
    _oddStrangerTurnBackOffRoot(task, retreat, ODD_STRANGER_BACK_OFF_TURN_LIMIT);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    // Only the retreat clip translates backward; alignment used the walk clip.
    if (work->animId == ODD_STRANGER_BACK_OFF_ANIM) {
        work->stateTimer++;
        if ((_playerDetectionOutOfReach(task->extra.tmd->coords, ODD_STRANGER_MOVE_STOP_DISTANCE, ODD_STRANGER_BACK_OFF_STEP) << 16) != 0) {
            _actorMovementStepForward(task->extra.tmd->coords, ODD_STRANGER_BACK_OFF_STEP);
        }
        if (_actorContactApplyGridPushback(task->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts)) != 1) {
            _oddStrangerApplyBodyPushback(task, work->hitContacts, ARRAY_SIZE(work->hitContacts));
        }
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        if (work->stateTimer >= ODD_STRANGER_BACK_OFF_TICKS) {
            if (work->lookYawTarget <= 0) {
                gfxRotMatrixY(&task->extra.tmd->coords->coord, ODD_STRANGER_BACK_OFF_EXIT_TURN, GRAPHICS_ROTATION_COMPOSE);
            } else {
                gfxRotMatrixY(&task->extra.tmd->coords->coord, -ODD_STRANGER_BACK_OFF_EXIT_TURN, GRAPHICS_ROTATION_COMPOSE);
            }
            work->state = ODD_STRANGER_STATE_CHASE;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}
