/* Part of the paced walk library; see paced_walk.h. */

/// Records a paced walker's requested clip and selects its next animation reseed.
///
/// Borrows live, writable `work` and a readable `request` for this call.
/// `animationId` is stored as a signed halfword without checking the clip bank.
/// Any nonzero `blend` selects a blended reseed and stores `blendFrames`, in
/// whole normal-rate frames, as a signed halfword. Reset selects an unblended
/// reseed and leaves the previous duration untouched. The request's bank
/// selector and world-collision choice are ignored.
///
/// Clears `st.field_6`, whose role is unproven. The caller must supply a clip
/// valid for its rig and perform the reseed; this only records the request,
/// leaving playing slots, the last applied clip, heading and travel unchanged.
/// Neither pointer is retained.
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
