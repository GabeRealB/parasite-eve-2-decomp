/* Part of the stride walk library; see stride_walk.h. */

/// Replaces draw and automatic-buffer flags on the walker and its carried model.
///
/// Requires a live TMD walker and `StrideWalkWork`. A nonzero spawn argument
/// requires a live carried-model task in `pairTask`; otherwise both references
/// name the walker itself. `ACTOR_MESSAGE_PAIR_SHOW` clears all model flags;
/// without it only active-draw exclusion is set. `ACTOR_MESSAGE_PAIR_SKIP_AUTO_BUFFER`
/// additionally suppresses automatic missing-buffer allocation. Other input
/// bits are ignored. Neither allocates nor releases buffers. Message ID and
/// second payload are ignored. Returns 0.
static s32 _strideWalkSetModelDraw(Task* task, s32 messageId, s32 drawFlags, s32 secondArg)
{
    StrideWalkWork* work;
    TmdObject*      model;
    TmdObject*      carriedModel;

    model = task->extra.tmd;
    work  = task->work;
    if (task->spawnArg1.value != 0) {
        carriedModel = work->pairTask->extra.tmd;
    } else {
        carriedModel = model;
    }
    if (drawFlags & ACTOR_MESSAGE_PAIR_SHOW) {
        model->flags        = 0;
        carriedModel->flags = 0;
    } else {
        model->flags        = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        carriedModel->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (drawFlags & ACTOR_MESSAGE_PAIR_SKIP_AUTO_BUFFER) {
        model->flags        |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        carriedModel->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    return 0;
}
