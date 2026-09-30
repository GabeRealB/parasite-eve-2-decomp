/* Part of the actor messages library; see actor_messages.h. */

/// Message 2004 in one call: builds the root coordinate's matrix from the
/// placement's rotation with RotMatrix, then writes its position. Returns 0.
s32 actorMsgPlaceRotMatrix(Task* arg0, s32 arg1, ActorTransform* args)
{
    GfxCoord* coord = arg0->extra.tmd->coords;

    RotMatrix(&args->rot, &coord->coord);
    coord->coord.t[0]   = args->pos.vx;
    coord->coord.t[1]   = args->pos.vy;
    coord->coord.t[2]   = args->pos.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return 0;
}
