/* Part of the stride walk library; see stride_walk.h. */

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
