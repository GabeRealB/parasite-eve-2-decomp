/* Part of the actor messages library; see actor_messages.h. */

s32 actorMsgPlaceEulerZyx(Task* task, s32 msgId, const ActorTransform* placement, s32 unusedArg)
{
    GfxCoord* rootCoord;

    rootCoord               = task->extra.tmd->coords;
    rootCoord->coord.t[0]   = placement->pos.vx;
    rootCoord->coord.t[1]   = placement->pos.vy;
    rootCoord->coord.t[2]   = placement->pos.vz;
    rootCoord->param.rot.vx = placement->rot.vx;
    rootCoord->param.rot.vy = placement->rot.vy;
    rootCoord->param.rot.vz = placement->rot.vz;
    RotMatrixZYX(&rootCoord->param.rot, &rootCoord->coord);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    return 0;
}
