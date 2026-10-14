/* Part of the paced walk library; see paced_walk.h. */

#ifndef SRC_SHARED_PACED_WALK_PAIR_DRAW_FLAGS
#define SRC_SHARED_PACED_WALK_PAIR_DRAW_FLAGS
/// Replaces a paced walker and its paired model's flags from a draw request.
///
/// Both pointers must refer to live, writable TMD objects and may alias.
/// `requestFlags` uses `ACTOR_MESSAGE_PAIR_*` bits, not `TMD_OBJECT_*` masks:
/// SHOW clears all existing object flags; without it, both models receive
/// only `TMD_OBJECT_SKIP_ACTIVE_DRAW`. SKIP_AUTO_BUFFER independently adds
/// `TMD_OBJECT_SKIP_AUTO_BUFFER`. Other request bits are ignored.
static inline void _pacedWalkReplacePairDrawFlags(TmdObject* walkerModel, TmdObject* pairModel, s32 requestFlags)
{
    enum { PACED_WALK_PAIR_MODEL_NO_FLAGS = 0 };

    if (requestFlags & ACTOR_MESSAGE_PAIR_SHOW) {
        walkerModel->flags = PACED_WALK_PAIR_MODEL_NO_FLAGS;
        pairModel->flags   = PACED_WALK_PAIR_MODEL_NO_FLAGS;
    } else {
        walkerModel->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        pairModel->flags   = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (requestFlags & ACTOR_MESSAGE_PAIR_SKIP_AUTO_BUFFER) {
        walkerModel->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        pairModel->flags   |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
}
#endif

/// Replaces the paced walker and its optional carried model's draw flags.
///
/// Handles `ACTOR_MESSAGE_SET_MODEL_DRAW` on a live, writable TMD task. Nonzero
/// `Task::spawnArg1.value` requires live `PacedWalkWork` at `Task::work` with a
/// live TMD task in `pairTask`; zero targets the receiver's model twice without
/// dereferencing the work block. Neither model pointer is retained.
///
/// `requestFlags` uses `ACTOR_MESSAGE_PAIR_*` bits. SHOW clears every model flag;
/// its absence replaces them with active-draw exclusion. The independent
/// `ACTOR_MESSAGE_PAIR_SKIP_AUTO_BUFFER` bit adds automatic-buffer suppression.
/// Other request bits are ignored. Neither allocates nor releases buffers.
/// The message ID and second payload are ignored. Returns 0.
static s32 PACED_WALK_SET_PAIR_MODEL_DRAW(Task* task, s32 messageId, s32 requestFlags, s32 unusedArg)
{
    PacedWalkWork* work;
    TmdObject*     walkerModel;
    TmdObject*     pairModel;

    walkerModel = task->extra.tmd;
    work        = task->work;
    if (task->spawnArg1.value != 0) {
        pairModel = work->pairTask->extra.tmd;
    } else {
        pairModel = walkerModel;
    }
    _pacedWalkReplacePairDrawFlags(walkerModel, pairModel, requestFlags);
    return 0;
}
