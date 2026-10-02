/* Part of the actor messages library; see actor_messages.h. */

/// Message 0x7D4 handler: places the task's model in the world. The model's
/// coordinate is parented to the view coordinate, takes `placement`'s
/// position as its translation and its rotation applied Y, then X, then Z.
/// `arg1` is the message id.
void actorMsgPlaceInView(Task* task, s32 arg1, ActorTransform* placement)
{
    GfxCoord* coord;
    MATRIX*   mtx;

    coord             = task->extra.tmd->coords;
    coord->parent     = &gGfxViewCoord;
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    mtx               = &coord->coord;
    coord->coord.t[2] = placement->pos.vz;
    gfxRotMatrixY(mtx, placement->rot.vy, 1);
    gfxRotMatrixX(mtx, placement->rot.vx, GRAPHICS_ROTATION_COMPOSE);
    Gfx_RotMatrixZ(mtx, placement->rot.vz, 0);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}
