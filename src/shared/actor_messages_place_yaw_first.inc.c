/* Part of the actor messages library; see actor_messages.h. */

/// Message 2004 with the yaw applied first: writes `placement` onto the root
/// coordinate (translation, then yaw, pitch and roll), then stores the
/// resulting heading from the matrix's third row in
/// `ActorMsgYawWork::placedYaw`. Returns 1.
///
/// The `TmdObject` is re-read from `Task::extra` for every access because the
/// stores and the `Gfx_RotMatrix*` calls in between may alias it.
s32 actorMsgPlaceYawFirst(Task* task, s32 arg1, ActorTransform* placement, s32 arg3)
{
    ActorMsgYawWork* work;

    work                                = task->work;
    task->extra.tmd->coords->coord.t[0] = placement->pos.vx;
    task->extra.tmd->coords->coord.t[1] = placement->pos.vy;
    task->extra.tmd->coords->coord.t[2] = placement->pos.vz;
    gfxRotMatrixY(&task->extra.tmd->coords->coord, placement->rot.vy, 1);
    gfxRotMatrixX(&task->extra.tmd->coords->coord, placement->rot.vx, GRAPHICS_ROTATION_COMPOSE);
    gfxRotMatrixZ(&task->extra.tmd->coords->coord, placement->rot.vz, GRAPHICS_ROTATION_COMPOSE);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    work->placedYaw                       = ratan2(-task->extra.tmd->coords->coord.m[2][0],
                                                   task->extra.tmd->coords->coord.m[2][2]);
    return 1;
}
