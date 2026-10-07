/* Part of the Odd Stranger library; see odd_stranger.h. */

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
#if ODD_STRANGER_BODY2_GRID
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
