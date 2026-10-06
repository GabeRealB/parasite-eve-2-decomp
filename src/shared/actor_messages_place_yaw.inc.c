/* Part of the actor messages library; see actor_messages.h. */

/// Applies the XYZ placement to the root, re-reading the task body across stores and calls.
static __inline__ void _actorMsgPlaceRecordYawTransform(Task* task, const ActorTransform* placement)
{
    task->extra.tmd->coords->coord.t[0] = placement->pos.vx;
    task->extra.tmd->coords->coord.t[1] = placement->pos.vy;
    task->extra.tmd->coords->coord.t[2] = placement->pos.vz;
    gfxRotMatrixX(&task->extra.tmd->coords->coord, placement->rot.vx, GRAPHICS_ROTATION_REPLACE);
    gfxRotMatrixY(&task->extra.tmd->coords->coord, placement->rot.vy, GRAPHICS_ROTATION_COMPOSE);
    gfxRotMatrixZ(&task->extra.tmd->coords->coord, placement->rot.vz, GRAPHICS_ROTATION_COMPOSE);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

s32 actorMsgPlaceRecordYaw(Task* task, s32 msgId, const ActorTransform* placement, s32 unusedArg)
{
    ActorMsgYawWork* work;

    work = task->work;
    _actorMsgPlaceRecordYawTransform(task, placement);
    // Record the final orientation, including pitch and roll, rather than input yaw.
    work->placedYaw = ratan2(-task->extra.tmd->coords->coord.m[2][0],
                             task->extra.tmd->coords->coord.m[2][2]);
    return 1;
}
