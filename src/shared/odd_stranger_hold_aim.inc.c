/* Part of the Odd Stranger library; see odd_stranger.h. */

/// Keeps the root's current heading and restores the normal model scale.
///
/// Borrows a live model and reserved chase scratch; yaw uses 4096 units per turn.
static __inline__ void _oddStrangerTurnWatchRoot(Task* task, ActorChaseScratch* watch)
{
    GfxCoord* headingRoot;

    // Both signs reduce to zero; the full player turn remains in the look target.
    if (watch->turn > 0) {
        watch->turn = 0;
    }
    if (watch->turn < 0) {
        watch->turn = 0;
    }
    headingRoot  = task->extra.tmd->coords;
    watch->turn += ratan2(-headingRoot->coord.m[2][0], headingRoot->coord.m[2][2]);
    gfxRotMatrixY(&task->extra.tmd->coords->coord, watch->turn, GRAPHICS_ROTATION_REPLACE);
    _actorRenderRescaleYaw(task->extra.tmd->coords, ODD_STRANGER_ROOT_SCALE);
}

/// Watches the player without turning the root, then resumes the chase.
///
/// Handles `ODD_STRANGER_STATE_WATCH` on a live Odd Stranger task with bound
/// rigs and a live player in the same root-parent space. The full relative
/// bearing updates the look target, but both turn clamps reduce root rotation
/// to zero, preserving its heading while restoring the normal model scale.
/// The primary clip boundary selects `CHASE` before playback advances.
/// Borrows one chase scratch block plus the yaw-rescale workspace.
static void _oddStrangerWatch(Task* task)
{
    OddStrangerWork*   work;
    ActorChaseScratch* watch;
    TmdObject*         model;

    work = task->work;
    if (work->stateEntered != 0) {
        model                                                     = task->extra.tmd;
        ((Enemy*)task->spawnArg2.pointer)->node.state.parts.flags = 0;
        model->flags                                              = 0;
        tmdAllocPrimitiveBuffer(model);
        work->hitBody.radius    = ODD_STRANGER_BODY_RADIUS;
        work->animRequest       = ODD_STRANGER_ANIM_REQUEST_BLEND;
        work->animRate          = ANIMATION_RATE_ONE;
        work->animId            = ODD_STRANGER_HOLD_AIM_CLIP;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
#if ODD_STRANGER_BODY2_GRID
        work->gridBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
#else
        work->gridBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
#endif
        _oddStrangerDriveAnimation(task);
        work->stateTimer = 0;
        return;
    }
    work->stateTimer += 1;
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    watch                                 = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->state = ODD_STRANGER_STATE_CHASE;
    }
    watch->turn         = _actorAngleTurnToPlayer(task, &watch->delta, &gPlayerStatus);
    work->lookYawTarget = watch->turn;
    _oddStrangerTurnWatchRoot(task, watch);
    _oddStrangerDriveAnimation(task);
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}
