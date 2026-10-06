/* Part of the pair walk library; see pair_walk.h. */

/// Replaces the walker and carried model's draw flags together.
///
/// Handles `ACTOR_MESSAGE_SET_MODEL_DRAW` with a live TMD walker, `PairWalkWork`
/// and its live `pairTask`. `ACTOR_MESSAGE_PAIR_SHOW` clears every flag on both
/// models; without it both receive only active-draw exclusion.
/// `ACTOR_MESSAGE_PAIR_SKIP_AUTO_BUFFER` adds automatic-buffer suppression.
/// Other request bits are ignored; no buffer is allocated or released.
/// The message ID and second payload are ignored. Returns 0.
static s32 _pairWalkSetVisibility(Task* task, s32 messageId, s32 flags, s32 unusedArg)
{
    TmdObject*    model;
    TmdObject*    carriedModel;
    PairWalkWork* work;

    model        = task->extra.tmd;
    work         = task->work;
    carriedModel = work->pairTask->extra.tmd;

    if (flags & ACTOR_MESSAGE_PAIR_SHOW) {
        model->flags        = 0;
        carriedModel->flags = 0;
    } else {
        model->flags        = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        carriedModel->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }

    if (flags & ACTOR_MESSAGE_PAIR_SKIP_AUTO_BUFFER) {
        model->flags        |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        carriedModel->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    return 0;
}
