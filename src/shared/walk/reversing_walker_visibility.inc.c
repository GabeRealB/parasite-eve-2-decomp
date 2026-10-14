/* Part of the reversing walker library; see reversing_walker.h. */

/// Sets model drawing and buffer recovery, with optional delayed buffer release.
///
/// Handles `ACTOR_MESSAGE_SET_MODEL_DRAW` on a live TMD task. Mode 0 hides and
/// enables automatic buffer recovery; 1 shows, allocates the buffer and enables
/// recovery; 2 hides, disables recovery and sets `ReverseWalkWork::freeCountdown`
/// to 2; 3 shows with recovery disabled. Mode 2 requires initialized work; the
/// update entering with a zero countdown frees the buffer. Other modes change
/// nothing and return 1; handled modes return 0. Unrelated flags are retained.
/// Showing does not cancel a pending release. Ignores message ID and second
/// payload, and retains no payload storage.
static s32 _reverseWalkSetDrawModeMsg(Task* task, s32 messageId, s32 mode, s32 unusedArg)
{
    enum { REVERSE_WALK_DRAW_SHOW_SKIP_AUTO_BUFFER = 3 };
    TmdObject* model;
    s32        result;

    model  = task->extra.tmd;
    result = 0;
    switch (mode) {
        case ACTOR_MESSAGE_DRAW_HIDE:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            model->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_MESSAGE_DRAW_SHOW:
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            tmdAllocPrimitiveBuffer(model);
            model->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER: {
            ReverseWalkWork* work;

            model->flags       |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work                = task->work;
            work->freeCountdown = mode;
            model->flags       |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        }
        case REVERSE_WALK_DRAW_SHOW_SKIP_AUTO_BUFFER:
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            result = 1;
            break;
    }
    return result;
}
