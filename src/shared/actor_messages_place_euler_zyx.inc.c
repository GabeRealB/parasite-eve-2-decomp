/* Part of the actor messages library; see actor_messages.h. */

/// `actorMsgPlaceEuler` with the angles applied Z, Y then X: the placement's
/// position becomes the translation, its angles are kept in `param.rot` and
/// turned into the matrix by `RotMatrixZYX`, and the coordinate is marked dirty.
/// Returns 0.
s32 actorMsgPlaceEulerZyx(Task* task, s32 arg1, ActorTransform* placement, s32 arg3)
{
    GfxCoord* coord;

    coord               = task->extra.tmd->coords;
    coord->coord.t[0]   = placement->pos.vx;
    coord->coord.t[1]   = placement->pos.vy;
    coord->coord.t[2]   = placement->pos.vz;
    coord->param.rot.vx = placement->rot.vx;
    coord->param.rot.vy = placement->rot.vy;
    coord->param.rot.vz = placement->rot.vz;
    RotMatrixZYX(&coord->param.rot, &coord->coord);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return 0;
}
