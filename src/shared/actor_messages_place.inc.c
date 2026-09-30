/* Part of the actor messages library; see actor_messages.h. */

/// Places the task's model from `placement`: the three longs become the
/// coordinate's translation, then the X, Y and Z angles are applied in that
/// order and the coordinate is marked dirty. Always returns 1.
s32 actorMsgPlace(Task* task, s32 arg1, ActorTransform* placement)
{
    task->extra.tmd->coords->coord.t[0] = placement->pos.vx;
    task->extra.tmd->coords->coord.t[1] = placement->pos.vy;
    task->extra.tmd->coords->coord.t[2] = placement->pos.vz;
    Gfx_RotMatrixX(&task->extra.tmd->coords->coord, placement->rot.vx, 1);
    Gfx_RotMatrixY(&task->extra.tmd->coords->coord, placement->rot.vy, 0);
    Gfx_RotMatrixZ(&task->extra.tmd->coords->coord, placement->rot.vz, 0);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    return 1;
}
