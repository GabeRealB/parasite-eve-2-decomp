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

/// Places the walker root at a borrowed position and yaw.
///
/// Handles `ACTOR_MESSAGE_PLACE` on a live TMD walker with `PairWalkWork`.
/// Position uses its existing root parent's coordinates; yaw uses 4096 units
/// per turn. Reads only XYZ and Y rotation, replaces the local rotation with
/// that yaw, records it in `st.yaw` and invalidates composition. The readable,
/// word-aligned payload is consumed during dispatch and is not retained.
/// The message ID and second payload are ignored. Returns 0.
static s32 _pairWalkPlace(Task* task, s32 messageId, const ActorTransform* placement, s32 unusedArg)
{
    GfxCoord*     rootCoord;
    PairWalkWork* work;
    s16           yaw;

    rootCoord    = task->extra.tmd->coords;
    work         = task->work;
    yaw          = placement->rot.vy;
    work->st.yaw = yaw;
    gfxRotMatrixY(&rootCoord->coord, yaw, GRAPHICS_ROTATION_REPLACE);
    rootCoord->coord.t[0]   = placement->pos.vx;
    rootCoord->coord.t[1]   = placement->pos.vy;
    rootCoord->coord.t[2]   = placement->pos.vz;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    return 0;
}
