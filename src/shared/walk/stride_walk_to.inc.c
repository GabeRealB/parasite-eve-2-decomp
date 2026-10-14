/* Part of the stride walk library; see stride_walk.h. */

/// Faces the model root toward a target and records its attempted travel steps.
///
/// Requires the spawned TMD walker and writable `StrideWalkWork`. Borrows only
/// target X/Z in the root parent's coordinate frame, ignoring height and angles.
/// Replaces root rotation with unit-scale yaw (4096 units per turn), preserving
/// translation and the existing composition stamp. Divides the integer horizontal
/// distance estimate by 30 and stores its low signed halfword as the travel
/// count, discarding a short remainder. Offsets and their squared sum must fit
/// the nonnegative signed 32-bit square-root input. No target pointer is
/// retained. Does not select a clip or request a reseed; movement
/// requires a separate WALK play request. Ignores message ID and second payload
/// and returns 0. Rotation requires initialized scratch/GTE state.
static s32 _strideWalkSetWalkTarget(Task* task, s32 messageId, const ActorTransform* target, s32 secondArg)
{
    GfxCoord*       rootCoord;
    StrideWalkWork* work;
    s32             deltaX;
    s32             deltaZ;
    s16             yaw;

    rootCoord    = task->extra.tmd->coords;
    work         = task->work;
    deltaX       = target->pos.vx - rootCoord->coord.t[0];
    deltaZ       = target->pos.vz - rootCoord->coord.t[2];
    yaw          = ratan2(deltaX, deltaZ);
    work->st.yaw = yaw;
    gfxRotMatrixY(&rootCoord->coord, yaw, GRAPHICS_ROTATION_REPLACE);
    work->st.travel = SquareRoot0(deltaX * deltaX + deltaZ * deltaZ) / STRIDE_WALK_STEP_DISTANCE;
    return 0;
}

/// Attaches the carried model to walker part 7 and refreshes its root composition.
///
/// `task` owns a live TMD model with a root coordinate; its parent owns the
/// spawned stride walker, at least eight model coordinates and `StrideWalkWork`.
/// State 0 borrows the parent's lighting matrices and part-7 coordinate, then
/// enters state 1. Both states mark the carried root dirty; other states do
/// nothing. Parent model and work storage must outlive this child task, which
/// the spawn state joins to the parent's teardown tree. No allocation occurs.
static void _strideWalkSubModelTask(Task* task)
{
    enum {
        STRIDE_WALK_SUB_MODEL_ATTACH   = 0,
        STRIDE_WALK_SUB_MODEL_FOLLOW   = 1,
        STRIDE_WALK_CARRIED_MODEL_PART = 7,
    };

    Task*           parentTask      = task->parent;
    TmdObject*      model           = task->extra.tmd;
    GfxCoord*       rootCoord       = model->coords;
    GfxCoord*       attachmentCoord = &parentTask->extra.tmd->coords[STRIDE_WALK_CARRIED_MODEL_PART];
    StrideWalkWork* work            = parentTask->work;

    switch (task->state) {
        case STRIDE_WALK_SUB_MODEL_ATTACH:
            rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            model->lightMtx         = &work->light;
            model->colorMtx         = &work->color;
            rootCoord->parent       = attachmentCoord;
            task->state++;
            break;
        case STRIDE_WALK_SUB_MODEL_FOLLOW:
            rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            break;
    }
}
