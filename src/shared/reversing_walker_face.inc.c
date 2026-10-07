/* Part of the reversing walker library; see reversing_walker.h. */

/// Installs a root rotation in both its stored Euler angles and local matrix.
///
/// Requires a live coordinate and a separate readable rotation, in 4096 units
/// per turn. Translation is retained and composition is invalidated.
static inline void _reverseWalkApplyFacingRotation(GfxCoord* rootCoord, const SVECTOR* rotation)
{
    rootCoord->param.rot.vx = rotation->vx;
    rootCoord->param.rot.vy = rotation->vy;
    rootCoord->param.rot.vz = rotation->vz;
    RotMatrix(&rootCoord->param.rot, &rootCoord->coord);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Faces the root toward the destination, or almost half a turn away for backing.
///
/// Walk step 0 requires live work and a root coordinate with translation in the
/// destination's frame. Normalizes the XYZ offset and obtains yaw in 4096 units
/// per turn. Backing adds 2047 units, one unit short of half a turn.
/// Replaces pitch, roll and scale with a yaw rotation, invalidates composition
/// and advances to step 1.
static void _reverseWalkOrientForWalk(Task* task)
{
    ReverseWalkWork* work;
    GfxCoord*        rootCoord;
    VECTOR           targetOffset;
    SVECTOR          targetDirection;
    SVECTOR          facingRotation;

    work      = task->work;
    rootCoord = task->extra.tmd->coords;

    targetOffset.vx = work->walk.target.vx - rootCoord->coord.t[0];
    targetOffset.vy = work->walk.target.vy - rootCoord->coord.t[1];
    targetOffset.vz = work->walk.target.vz - rootCoord->coord.t[2];
    VectorNormalS(&targetOffset, &targetDirection);

    facingRotation.vx = 0;
    facingRotation.vy = ratan2(targetDirection.vx, targetDirection.vz);
    facingRotation.vz = 0;
    if (work->walksForward == 0) {
        facingRotation.vy += ACTOR_TRANSFORM_ANGLE_HALF_TURN - 1;
    }

    _reverseWalkApplyFacingRotation(rootCoord, &facingRotation);
    work->walk.motionStep++;
}
