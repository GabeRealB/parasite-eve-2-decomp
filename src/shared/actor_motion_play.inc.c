/* Part of the actor motion library; see actor_motion.h. */

#include "actor_motion_play_helpers.h"

/// Starts the requested clip on the carrier's twenty-part model, even if repeated.
///
/// Handles `ACTOR_MESSAGE_PLAY_ANIMATION` on a live TMD task whose work opens
/// as `ActorMotionPlayWork` does. Initialize `model.bank` to
/// `ACTOR_MODEL_STATE_NONE` before the first request so the rig is bound.
/// Bank and clip must index loaded entries of `gActorMotionAnimBanks` and fit
/// nonnegative signed bytes. The request is borrowed through dispatch and must
/// not overlap playback storage; model coordinates, work-owned slots/poses and
/// clip tables remain borrowed while playback uses them. Slots 1..19 blend for
/// `blendFrames` whole frames (normally 0..2047) if already ticking and requested,
/// or reset otherwise, then tick once. Ignores message ID, collision choice and
/// fourth argument. Returns 0; subsequent frame ticking belongs to the carrier.
static s32 _actorMotionPlayAnim(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg)
{
    ActorMotionPlayWork* work;
    TmdObject*           model;

    work  = task->work;
    model = task->extra.tmd;
    _actorMotionApplyAnimationRequest(work, model, request);
    return 0;
}
