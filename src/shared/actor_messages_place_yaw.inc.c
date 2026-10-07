/* Part of the actor messages library; see actor_messages.h. */

/// Replaces the model root's local transform with an XYZ placement.
///
/// Requires a live TMD task with a writable root coordinate and a readable,
/// word-aligned `ActorTransform` through the call. Position uses the existing
/// root parent's coordinate frame; angles use 4096 units per turn and need not
/// be normalized. Requires an initialized graphics scratch stack. Installs
/// Rx * Ry * Rz and invalidates composition without changing the parent or
/// stored Euler angles. Reads only vector X/Y/Z and retains no payload pointer.
static __inline__ void _actorMsgApplyXyzPlacement(Task* task, const ActorTransform* placement)
{
    task->extra.tmd->coords->coord.t[0] = placement->pos.vx;
    task->extra.tmd->coords->coord.t[1] = placement->pos.vy;
    task->extra.tmd->coords->coord.t[2] = placement->pos.vz;
    gfxRotMatrixX(&task->extra.tmd->coords->coord, placement->rot.vx, GRAPHICS_ROTATION_REPLACE);
    gfxRotMatrixY(&task->extra.tmd->coords->coord, placement->rot.vy, GRAPHICS_ROTATION_COMPOSE);
    gfxRotMatrixZ(&task->extra.tmd->coords->coord, placement->rot.vz, GRAPHICS_ROTATION_COMPOSE);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

s32 ACTOR_MESSAGE_PLACE_RECORD_YAW(Task* task, s32 msgId, const ActorTransform* placement, s32 unusedArg)
{
    ACTOR_MESSAGE_YAW_WORK_TYPE* work;

    work = task->work;
    _actorMsgApplyXyzPlacement(task, placement);
    // Record the final orientation, including pitch and roll, rather than input yaw.
    work->placedYaw = ratan2(-task->extra.tmd->coords->coord.m[2][0],
                             task->extra.tmd->coords->coord.m[2][2]);
    return 1;
}
