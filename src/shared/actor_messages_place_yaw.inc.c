/* Part of the actor messages library; see actor_messages.h. */

/// Message 2004: places the model like actorMsgPlace, then stores the resulting
/// heading (from the root matrix's Z axis) in the work block's yaw. Returns 1.
s32 actorMsgPlaceRecordYaw(Task* task, s32 arg1, ActorTransform* placement)
{
    GfxCoord*     coord;
    s32           mx;
    s32           mz;
    ActorYawWork* work;

    work                                = (ActorYawWork*)task->work;
    task->extra.tmd->coords->coord.t[0] = placement->pos.vx;
    task->extra.tmd->coords->coord.t[1] = placement->pos.vy;
    task->extra.tmd->coords->coord.t[2] = placement->pos.vz;
    Gfx_RotMatrixX(&task->extra.tmd->coords->coord, placement->rot.vx, 1);
    Gfx_RotMatrixY(&task->extra.tmd->coords->coord, placement->rot.vy, 0);
    Gfx_RotMatrixZ(&task->extra.tmd->coords->coord, placement->rot.vz, 0);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    coord                                 = task->extra.tmd->coords;
    mx                                    = coord->coord.m[2][0];
    mz                                    = coord->coord.m[2][2];
    work->yaw                             = ratan2(-mx, mz);
    return 1;
}
