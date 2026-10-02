/* Part of the actor messages library; see actor_messages.h. */

/// Message 0x7D4 handler of both of the overlay's message tables: copies the
/// placement onto the model's root coordinate, the three longs as its
/// translation and the three angles as its rotation (Y, then X, then Z), and
/// marks the coordinate dirty.
void actorMsgPlaceYawPitchRoll(Task* task, s32 arg1, ActorTransform* placement)
{
    GfxCoord* coord;
    MATRIX*   mtx;

    coord             = task->extra.tmd->coords;
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    mtx               = &coord->coord;
    coord->coord.t[2] = placement->pos.vz;
    gfxRotMatrixY(mtx, placement->rot.vy, 1);
    gfxRotMatrixX(mtx, placement->rot.vx, GRAPHICS_ROTATION_COMPOSE);
    Gfx_RotMatrixZ(mtx, placement->rot.vz, 0);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}
