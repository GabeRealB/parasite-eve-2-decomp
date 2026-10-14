/* Part of the stride walk library; see stride_walk.h. */

/// Immediately reseeds the walker's non-root tracks from a scripted play request.
///
/// Requires the spawned TMD walker and live `StrideWalkWork` rig. `request`
/// is borrowed only for this call. Clip IDs 1..11 select loaded sets; slot 0
/// of the bank is NULL. The retained check rejects only IDs >=12, so callers
/// must also exclude zero and negative IDs. Reset restarts the tracks; nonzero
/// `blend` captures their old poses and takes `blendFrames`' low signed
/// halfword as the duration in whole normal-rate frames; 0..2047 keeps the
/// slots' signed transition time nonnegative. Travel is unchanged.
/// Ignores source, collision choice, message ID and second payload. Returns
/// 0 after reseeding or -1 for an out-of-bank ID, leaving state unchanged.
static s32 _strideWalkPlayAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 secondArg)
{
    enum {
        STRIDE_WALK_ANIMATION_ID_LIMIT = ARRAY_SIZE(_gStrideWalkAnimBank),
        STRIDE_WALK_PLAY_ACCEPTED      = 0,
        STRIDE_WALK_PLAY_REJECTED      = -1,
    };
    StrideWalkWork* work;

    work = task->work;
    if (request->animationId < STRIDE_WALK_ANIMATION_ID_LIMIT) {
        work->st.animId = request->animationId;
        if (request->blend != ANIMATION_BLEND_RESET) {
            work->st.state    = ACTOR_ENEMY_ANIM_BLEND;
            work->blendFrames = request->blendFrames;
        } else {
            work->st.state = ACTOR_ENEMY_ANIM_RESET;
        }
        work->st.field_6 = 0;
        _strideWalkUpdate(task);
        return STRIDE_WALK_PLAY_ACCEPTED;
    }
    return STRIDE_WALK_PLAY_REJECTED;
}

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
