/* Part of the paced walk library; see paced_walk.h. */

/// Records the clip and transition choice for an immediate animation reseed.
///
/// Borrows writable work and a readable request through this call. The caller
/// supplies a loaded clip and applies the recorded request. Clip and blend duration
/// are narrowed to signed halfwords; reset leaves the old duration untouched.
/// Clears the unproven request field `st.field_6`. Retains no request pointer
/// and does not change the playing tracks or remaining travel.
static inline void _pacedWalkApplyAnimationRequest(PacedWalkWork* work, const AnimationPlayRequest* request)
{
    work->st.animId = request->animationId;
    if (request->blend != ANIMATION_BLEND_RESET) {
        work->st.state    = ACTOR_ENEMY_ANIM_BLEND;
        work->blendFrames = request->blendFrames;
    } else {
        work->st.state = ACTOR_ENEMY_ANIM_RESET;
    }
    work->st.field_6 = 0;
}

/// Immediately reseeds a paced walker's non-root tracks from a scripted play request.
///
/// Handles `ACTOR_MESSAGE_PLAY_ANIMATION` for a spawned TMD task with a live
/// `PacedWalkWork` rig and loaded clips 1..15. The signed check rejects only
/// IDs >=16; callers must exclude negative IDs and the unloaded ID 0. Borrows
/// the word-aligned request through dispatch without retaining it; the rig's
/// coordinates, pose buffers and clip storage must remain live during playback.
///
/// Nonzero `blend` captures the previous poses and narrows `blendFrames` to a
/// signed halfword in whole normal-rate frames; 0..2047 keeps the animation
/// slots' remaining blend time nonnegative. Reset ignores the duration. The
/// selected `PACED_WALK_UPDATE` reseeds parts 1..19 and enters normal ticking,
/// without advancing travel. Root part 0 is unchanged. Scratch/GTE requirements
/// follow the selected reseed helper. The request's bank selector and collision
/// choice, `messageId` and `unusedArgument` are ignored. Returns 0 after
/// reseeding or -1 for a rejected ID, with no playback changes on rejection.
static s32 _pacedWalkPlayAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArgument)
{
    enum {
        PACED_WALK_ANIMATION_ID_LIMIT         = 16,
        PACED_WALK_ANIMATION_REQUEST_APPLIED  = 0,
        PACED_WALK_ANIMATION_REQUEST_REJECTED = -1,
    };
    PacedWalkWork* work;

    work = task->work;
    if (request->animationId < PACED_WALK_ANIMATION_ID_LIMIT) {
        _pacedWalkApplyAnimationRequest(work, request);
        PACED_WALK_UPDATE(task);
        return PACED_WALK_ANIMATION_REQUEST_APPLIED;
    }
    return PACED_WALK_ANIMATION_REQUEST_REJECTED;
}
