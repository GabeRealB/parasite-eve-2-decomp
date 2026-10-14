/* Part of the actor messages library; see actor_messages.h. */

s32 actorMsgPlaceRotMatrix(Task* task, s32 msgId, const ActorTransform* placement, s32 unusedArg)
{
    GfxCoord* rootCoord = task->extra.tmd->coords;

    // The SDK reads the angles despite its unqualified input signature.
    RotMatrix((SVECTOR*)&placement->rot, &rootCoord->coord);
    rootCoord->coord.t[0]   = placement->pos.vx;
    rootCoord->coord.t[1]   = placement->pos.vy;
    rootCoord->coord.t[2]   = placement->pos.vz;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    return 0;
}
