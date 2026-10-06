/* Part of the pair walk library; see pair_walk.h. */

/// Aims the walker at a borrowed destination and records its remaining travel ticks.
///
/// Handles `ACTOR_MESSAGE_WALK_TO` on a live TMD walker with `PairWalkWork`.
/// Reads only `target->pos.vx` and `target->pos.vz` in the root parent's frame;
/// Y and rotation are ignored. Replaces the root's rotation and records its
/// yaw in 4096 units per turn. Stores floor(horizontal distance / 12) in the
/// signed halfword `st.travel`, without clamping; the signed square sum must
/// be representable and the tick count must fit 0 to 32767. Does not invalidate
/// composition or select an animation: a separate play request starts walking.
/// The word-aligned payload is borrowed only through dispatch. The message ID
/// and second payload are ignored. Returns 0.
static s32 _pairWalkTo(Task* task, s32 messageId, const ActorTransform* target, s32 unusedArg)
{
    GfxCoord*     rootCoord;
    PairWalkWork* work;
    s32           deltaX;
    s32           deltaZ;
    s16           yaw;

    rootCoord    = task->extra.tmd->coords;
    work         = task->work;
    deltaX       = target->pos.vx - rootCoord->coord.t[0];
    deltaZ       = target->pos.vz - rootCoord->coord.t[2];
    yaw          = ratan2(deltaX, deltaZ);
    work->st.yaw = yaw;
    gfxRotMatrixY(&rootCoord->coord, yaw, GRAPHICS_ROTATION_REPLACE);
    work->st.travel = SquareRoot0(deltaX * deltaX + deltaZ * deltaZ) / PAIR_WALK_MODEL_STEP_UNITS;
    return 0;
}
