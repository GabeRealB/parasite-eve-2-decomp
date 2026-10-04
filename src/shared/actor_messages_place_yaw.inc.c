/* Part of the actor messages library; see actor_messages.h. */

/// Message 2004: places the model like actorMsgPlace, then stores the resulting
/// heading (from the root matrix's Z axis) in `ActorMsgYawWork::placedYaw`.
/// Returns 1.
s32 actorMsgPlaceRecordYaw(Task* task, s32 arg1, ActorTransform* placement, s32 arg3)
{
    GfxCoord*        coord;
    s32              mx;
    s32              mz;
    ActorMsgYawWork* work;

    work                                = task->work;
    task->extra.tmd->coords->coord.t[0] = placement->pos.vx;
    task->extra.tmd->coords->coord.t[1] = placement->pos.vy;
    task->extra.tmd->coords->coord.t[2] = placement->pos.vz;
    gfxRotMatrixX(&task->extra.tmd->coords->coord, placement->rot.vx, GRAPHICS_ROTATION_REPLACE);
    gfxRotMatrixY(&task->extra.tmd->coords->coord, placement->rot.vy, 0);
    gfxRotMatrixZ(&task->extra.tmd->coords->coord, placement->rot.vz, GRAPHICS_ROTATION_COMPOSE);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    coord                                 = task->extra.tmd->coords;
    mx                                    = coord->coord.m[2][0];
    mz                                    = coord->coord.m[2][2];
    work->placedYaw                       = ratan2(-mx, mz);
    return 1;
}
