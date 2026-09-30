/* Part of the actor messages library; see actor_messages.h. */

/// Message 0x7D4 handler, listed in `D_actor_361100_80171BB8`: places the
/// model at once. Writes the payload's translation into the root coordinate,
/// keeps its Euler angles in the coordinate's `rot` slot and rebuilds the
/// rotation from them with `RotMatrix`, then clears `composeStamp` so
/// `actorRenderComposeCoordChain` recomputes the composed matrix.
s32 actorMsgPlaceEuler(Task* task, s32 arg1, ActorTransform* placement, s32 arg3)
{
    GfxCoord* coord;

    coord               = task->extra.tmd->coords;
    coord->coord.t[0]   = placement->pos.vx;
    coord->coord.t[1]   = placement->pos.vy;
    coord->coord.t[2]   = placement->pos.vz;
    coord->param.rot.vx = placement->rot.vx;
    coord->param.rot.vy = placement->rot.vy;
    coord->param.rot.vz = placement->rot.vz;
    RotMatrix(&coord->param.rot, &coord->coord);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return 0;
}
