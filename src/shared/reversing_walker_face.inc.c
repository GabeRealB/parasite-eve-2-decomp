/* Part of the reversing walker library; see reversing_walker.h. */

/// State handler at index 0 of `D_actor_350700_80161E30`: turns the root part
/// toward `work->walk.target`, taking the yaw of the normalised offset from the
/// part's own translation with `ratan2` -- turned half a revolution away while
/// `walksForward` is clear -- and rebuilding the local matrix from that yaw alone.
/// Clearing `composeStamp` makes the coordinate tree recompute the world matrix, and
/// bumping `walk.motionStep` moves on to the next handler.
void reverseWalkFaceTarget(Task* task)
{
    ReverseWalkWork* work;
    GfxCoord*        coord;
    VECTOR           delta;
    SVECTOR          dir;
    SVECTOR          rot;

    work  = task->work;
    coord = task->extra.tmd->coords;

    delta.vx = work->walk.target.vx - coord->coord.t[0];
    delta.vy = work->walk.target.vy - coord->coord.t[1];
    delta.vz = work->walk.target.vz - coord->coord.t[2];
    VectorNormalS(&delta, &dir);

    rot.vx = 0;
    rot.vy = ratan2(dir.vx, dir.vz);
    rot.vz = 0;
    if (work->walksForward == 0) {
        rot.vy += 0x7FF;
    }

    coord->param.rot.vx = rot.vx;
    coord->param.rot.vy = rot.vy;
    coord->param.rot.vz = rot.vz;
    RotMatrix(&coord->param.rot, &coord->coord);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    work->walk.motionStep++;
}
