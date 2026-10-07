/* Part of the paced walk library; see paced_walk.h. */

#ifndef SRC_SHARED_PACED_WALK_PAIR_DRAW_FLAGS
#define SRC_SHARED_PACED_WALK_PAIR_DRAW_FLAGS
/// Replaces both models' draw flags from an `ACTOR_MESSAGE_PAIR_*` request.
///
/// The pointers may alias. Preserve each model's ordered replace and OR writes.
static inline void _pacedWalkReplacePairDrawFlags(TmdObject* model, TmdObject* pairModel, s32 flags)
{
    if (flags & ACTOR_MESSAGE_PAIR_SHOW) {
        model->flags     = 0;
        pairModel->flags = 0;
    } else {
        model->flags     = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        pairModel->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (flags & ACTOR_MESSAGE_PAIR_SKIP_AUTO_BUFFER) {
        model->flags     |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        pairModel->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
}
#endif

/// Replaces the paced walker and its optional carried model's draw flags.
///
/// Handles `ACTOR_MESSAGE_SET_MODEL_DRAW` on a live TMD task with
/// `PacedWalkWork` at `Task::work`. Nonzero `Task::spawnArg1.value` requires a live
/// TMD task in `PacedWalkWork::pairTask`; otherwise both writes target the
/// receiver's model. `ACTOR_MESSAGE_PAIR_SHOW` clears every model flag; its
/// absence replaces them with active-draw exclusion. The independent
/// `ACTOR_MESSAGE_PAIR_SKIP_AUTO_BUFFER` bit adds automatic-buffer suppression.
/// Other request bits are ignored. Neither allocates nor releases buffers.
/// The message ID and second payload are ignored. Returns 0.
s32 PACED_WALK_SET_PAIR_MODEL_DRAW(Task* task, s32 messageId, s32 flags, s32 unusedArg)
{
    PacedWalkWork* work;
    TmdObject*     model;
    TmdObject*     pairModel;

    model = task->extra.tmd;
    work  = task->work;
    if (task->spawnArg1.value != 0) {
        pairModel = work->pairTask->extra.tmd;
    } else {
        pairModel = model;
    }
    _pacedWalkReplacePairDrawFlags(model, pairModel, flags);
    return 0;
}
