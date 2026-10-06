/* Part of the actor messages library; see actor_messages.h. */

void actorMsgPlaceInView(Task* task, s32 msgId, const ActorTransform* placement, s32 unusedArg)
{
    GfxCoord* rootCoord;
    MATRIX*   rootMatrix;

    rootCoord             = task->extra.tmd->coords;
    rootCoord->parent     = &gGfxViewCoord;
    rootCoord->coord.t[0] = placement->pos.vx;
    rootCoord->coord.t[1] = placement->pos.vy;
    rootMatrix            = &rootCoord->coord;
    rootCoord->coord.t[2] = placement->pos.vz;
    gfxRotMatrixY(rootMatrix, placement->rot.vy, GRAPHICS_ROTATION_REPLACE);
    gfxRotMatrixX(rootMatrix, placement->rot.vx, GRAPHICS_ROTATION_COMPOSE);
    gfxRotMatrixZ(rootMatrix, placement->rot.vz, GRAPHICS_ROTATION_COMPOSE);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
}
