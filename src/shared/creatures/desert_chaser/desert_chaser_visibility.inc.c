/* Part of the Desert Chaser library; see desert_chaser.h. */

/// Applies a model visibility mode and selects hidden or the cutscene show animation.
///
/// Handles ACTOR_MESSAGE_SET_MODEL_DRAW. HIDE replaces the model flags with
/// draw exclusion; SHOW clears them and selects clip 13 with per-carrier cues. Both ensure a
/// primitive buffer exists. The two SKIP_AUTO_BUFFER modes select hidden,
/// retaining or clearing the other flags without allocating a buffer. Unknown
/// modes change nothing. msgId and unusedArg are ignored. Returns 0.
static s32 _desertChaserSetVisibility(Task* task, s32 msgId, s32 mode, s32 unusedArg)
{
    TmdObject*        model;
    DesertChaserWork* work;

    model = task->extra.tmd;
    work  = task->work;
    switch (mode) {
        case ACTOR_MESSAGE_VISIBILITY_HIDE:
            model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            tmdAllocPrimitiveBuffer(model);
            work->state = DESERT_CHASER_STATE_HIDDEN;
            break;
        case ACTOR_MESSAGE_VISIBILITY_SHOW:
            model->flags = 0;
            tmdAllocPrimitiveBuffer(model);
            work->state = DESERT_CHASER_STATE_SHOW_ANIMATION;
            break;
        case ACTOR_MESSAGE_VISIBILITY_KEEP_FLAGS_SKIP_AUTO_BUFFER:
            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            work->state   = DESERT_CHASER_STATE_HIDDEN;
            break;
        case ACTOR_MESSAGE_VISIBILITY_CLEAR_FLAGS_SKIP_AUTO_BUFFER:
            model->flags  = 0;
            work->state   = DESERT_CHASER_STATE_HIDDEN;
            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
    }
    return 0;
}
