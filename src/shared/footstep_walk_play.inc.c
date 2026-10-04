/* Part of the footstep walk library; see footstep_walk.h. */

/// "Start animation" opcode: the request's blend selects between the two start
/// paths the runner `footstepWalkUpdate` dispatches on, and only the blended one
/// carries a frame count, which it leaves in `gFootstepWalkBlendFrames`. The runner is then
/// run once on the task published in `gFootstepWalkTask`. Returns -1,
/// without touching the work block, when the clip id is 0x23 or more.
s32 footstepWalkPlay(Task* task, s32 arg1, AnimationPlayRequest* args, s32 arg3)
{
    if (args->animationId < 0x23) {
        gFootstepWalkWork->st.animId = args->animationId;
        if (args->blend != ANIMATION_BLEND_RESET) {
            gFootstepWalkWork->st.state = ACTOR_ENEMY_ANIM_BLEND;
            gFootstepWalkBlendFrames    = args->blendFrames;
        } else {
            gFootstepWalkWork->st.state = ACTOR_ENEMY_ANIM_RESET;
        }
        gFootstepWalkWork->st.field_6 = 0;
        footstepWalkUpdate(gFootstepWalkTask);
        return 0;
    }
    return -1;
}
