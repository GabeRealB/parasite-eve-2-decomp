/* Part of the Desert Chaser library; see desert_chaser.h. */

/// Restarts the scripted-animation state with the request's clip id.
///
/// Handles ACTOR_MESSAGE_PLAY_ANIMATION in cutscene and Water Tower builds.
/// Copies only animationId, narrowed to the work block's signed halfword;
/// the clip must belong to this carrier's bank. Other request fields, msgId
/// and unusedArg are ignored. The borrowed request is not retained. Returns 0.
static s32 _desertChaserMsgPlayAnim(Task* task, s32 msgId, const AnimationPlayRequest* request, s32 unusedArg)
{
    DesertChaserWork* work = task->work;

    work->animId    = request->animationId;
    work->state     = DESERT_CHASER_STATE_SCRIPT_ANIMATION;
    work->prevState = DESERT_CHASER_PREV_STATE_NONE;
    return 0;
}
