/* Part of the actor motion library; see actor_motion.h. */

/// Faces the twenty-part walk's root toward its destination and advances the step.
///
/// Requires initialized `ActorMotionWalkWork` and a live model root coordinate.
/// Target and root translation share the root parent's coordinate frame; their
/// XYZ offset is normalized before taking X/Z yaw in 4096 units per turn.
/// Replaces pitch, roll and scale with a pure yaw rotation, records that Euler
/// rotation in the coordinate parameters and marks composition dirty. Keeps
/// translation and leaves subsequent movement to the carrier's next step.
static void _actorMotionFaceTarget(Task* task)
{
    ActorMotionWalkWork* work;
    GfxCoord*            rootCoord;
    VECTOR               targetOffset;
    SVECTOR              direction;
    SVECTOR              rotation;

    work      = task->work;
    rootCoord = task->extra.tmd->coords;

    // Face the destination in the root parent's frame before movement begins.
    targetOffset.vx = work->walk.target.vx - rootCoord->coord.t[0];
    targetOffset.vy = work->walk.target.vy - rootCoord->coord.t[1];
    targetOffset.vz = work->walk.target.vz - rootCoord->coord.t[2];
    VectorNormalS(&targetOffset, &direction);

    rotation.vx = 0;
    rotation.vy = ratan2(direction.vx, direction.vz);
    rotation.vz = 0;

    rootCoord->param.rot.vx = rotation.vx;
    rootCoord->param.rot.vy = rotation.vy;
    rootCoord->param.rot.vz = rotation.vz;
    RotMatrix(&rootCoord->param.rot, &rootCoord->coord);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    work->walk.motionStep++;
}
