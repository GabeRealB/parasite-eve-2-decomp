/* Part of the actor motion library; see actor_motion.h. */

/// Walk step 0: turns the root part to face `target`, taking the yaw of the
/// normalised offset from the part's own translation with `ratan2` and
/// rebuilding the local matrix from that yaw alone, then advances the step.
void actorMotionFaceTarget(Task* task)
{
    ActorMotionWalkWork* work;
    GfxCoord*            coord;
    VECTOR               delta;
    SVECTOR              dir;
    SVECTOR              rot;

    work  = (ActorMotionWalkWork*)task->work;
    coord = task->extra.tmd->coords;

    delta.vx = work->walk.target.vx - coord->coord.t[0];
    delta.vy = work->walk.target.vy - coord->coord.t[1];
    delta.vz = work->walk.target.vz - coord->coord.t[2];
    VectorNormalS(&delta, &dir);

    rot.vx = 0;
    rot.vy = ratan2(dir.vx, dir.vz);
    rot.vz = 0;

    coord->param.rot.vx = rot.vx;
    coord->param.rot.vy = rot.vy;
    coord->param.rot.vz = rot.vz;
    RotMatrix(&coord->param.rot, &coord->coord);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    work->walk.motionStep++;
}
