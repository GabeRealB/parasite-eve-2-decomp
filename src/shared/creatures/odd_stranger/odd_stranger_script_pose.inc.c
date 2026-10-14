/* Part of the Odd Stranger library; see odd_stranger.h. */

/// Plays the walk clip in place for `ODD_STRANGER_STATE_PLAY_WALK`.
///
/// On entry restores drawing and targeting, resets the clip at normal rate,
/// and drops secondary blending and attack pairing. Grid collision follows
/// the carrier variant. Each call advances playback; there is no state exit.
static void _oddStrangerPlayWalk(Task* task)
{
    TmdObject*       model;
    OddStrangerWork* work;

    work = task->work;
    if (work->stateEntered != 0) {
        model                                                     = task->extra.tmd;
        ((Enemy*)task->spawnArg2.pointer)->node.state.parts.flags = 0;
        model->flags                                              = 0;
        tmdAllocPrimitiveBuffer(model);
        work->animRequest       = ODD_STRANGER_ANIM_REQUEST_RESET;
        work->animRate          = ANIMATION_RATE_ONE;
        work->animId            = ODD_STRANGER_ANIM_WALK;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
#if ODD_STRANGER_IN_PLACE_GRID_ENABLED
        work->gridBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
#else
        work->gridBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
#endif
        _oddStrangerDriveAnimation(task);
    } else {
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        _oddStrangerDriveAnimation(task);
    }
}

/// Plays the run clip in place for `ODD_STRANGER_STATE_PLAY_RUN`.
///
/// On entry restores drawing and targeting, resets the clip at normal rate,
/// and drops secondary blending and attack pairing. Grid collision follows
/// the carrier variant. Each call advances playback; there is no state exit.
static void _oddStrangerPlayRun(Task* task)
{
    TmdObject*       model;
    OddStrangerWork* work;

    work = task->work;
    if (work->stateEntered != 0) {
        model                                                     = task->extra.tmd;
        ((Enemy*)task->spawnArg2.pointer)->node.state.parts.flags = 0;
        model->flags                                              = 0;
        tmdAllocPrimitiveBuffer(model);
        work->animRequest       = ODD_STRANGER_ANIM_REQUEST_RESET;
        work->animRate          = ANIMATION_RATE_ONE;
        work->animId            = ODD_STRANGER_ANIM_RUN;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
#if ODD_STRANGER_IN_PLACE_GRID_ENABLED
        work->gridBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
#else
        work->gridBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
#endif
        _oddStrangerDriveAnimation(task);
    } else {
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        _oddStrangerDriveAnimation(task);
    }
}

/// Plays the lying-back clip for `ODD_STRANGER_STATE_PLAY_DOWN`.
///
/// On entry restores drawing and targeting, resets the clip at normal rate,
/// and drops secondary blending and attack pairing. Grid collision follows
/// the carrier variant. Each call advances playback; there is no state exit.
static void _oddStrangerPlayDown(Task* task)
{
    TmdObject*       model;
    OddStrangerWork* work;

    work = task->work;
    if (work->stateEntered != 0) {
        model                                                     = task->extra.tmd;
        ((Enemy*)task->spawnArg2.pointer)->node.state.parts.flags = 0;
        model->flags                                              = 0;
        tmdAllocPrimitiveBuffer(model);
        work->animRequest       = ODD_STRANGER_ANIM_REQUEST_RESET;
        work->animRate          = ANIMATION_RATE_ONE;
        work->animId            = ODD_STRANGER_ANIM_DOWN_BACK;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
#if ODD_STRANGER_IN_PLACE_GRID_ENABLED
        work->gridBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
#else
        work->gridBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
#endif
        _oddStrangerDriveAnimation(task);
    } else {
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        _oddStrangerDriveAnimation(task);
    }
}

/// Plays the damage flinch, then selects `ODD_STRANGER_STATE_CHASE`.
///
/// Entry restores drawing and targeting, resets the flinch clip at 18/16
/// frames per call, and drops blending and attack pairing. Grid collision
/// follows the carrier variant. Slot 1's boundary flag ends this state.
static void _oddStrangerFlinch(Task* task)
{
    TmdObject*       model;
    OddStrangerWork* work;

    work = task->work;
    if (work->stateEntered != 0) {
        model                                                     = task->extra.tmd;
        ((Enemy*)task->spawnArg2.pointer)->node.state.parts.flags = 0;
        model->flags                                              = 0;
        tmdAllocPrimitiveBuffer(model);
        work->animRequest       = ODD_STRANGER_ANIM_REQUEST_RESET;
        work->animRate          = ANIMATION_RATE_ONE + 2;
        work->animId            = ODD_STRANGER_ANIM_FLINCH;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
#if ODD_STRANGER_IN_PLACE_GRID_ENABLED
        work->gridBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
#else
        work->gridBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
#endif
    }
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    _oddStrangerDriveAnimation(task);
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->state = ODD_STRANGER_STATE_CHASE;
    }
}

/// Gets up from the back, then selects `ODD_STRANGER_STATE_CHASE`.
///
/// Entry restores drawing, targeting and the grid body, resets the rise clip
/// at `chaseRate` (sixteenths of a frame per call), and clears the look yaw.
/// Slot 1's boundary flag ends `ODD_STRANGER_STATE_RISE_BACK`.
static void _oddStrangerRiseBack(Task* task)
{
    OddStrangerWork* work;
    Enemy*           enemy;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        task->extra.tmd->flags        = 0;
        work->hitBody.radius          = ODD_STRANGER_BODY_RADIUS;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->animRequest             = ODD_STRANGER_ANIM_REQUEST_RESET;
        work->animId                  = ODD_STRANGER_ANIM_RISE_BACK;
        work->lookYaw                 = 0;
        work->lookYawTarget           = 0;
        work->animRate                = work->chaseRate;
    }
    _oddStrangerDriveAnimation(task);
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->state = ODD_STRANGER_STATE_CHASE;
    }
}
