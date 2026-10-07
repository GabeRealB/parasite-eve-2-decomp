/* Part of the Odd Stranger library; see odd_stranger.h. */

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
#if ODD_STRANGER_BODY2_GRID
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
