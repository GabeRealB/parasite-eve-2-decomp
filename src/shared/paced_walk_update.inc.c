/* Part of the paced walk library; see paced_walk.h. */

/// The actor's step body. States 1 and 2 reseed the animation slots (blending
/// over `blendFrames` and from the clip's start) and advance to 3; state 3 walks the root coordinate 12
/// units per frame while the walk clip has `travel` left, switching to clip 1
/// with argument 0xA when it runs out, then ticks the slots.
void pacedWalkUpdate(Task* task)
{
    PacedWalkWork* work;
    s16            animId;

    work = task->work;
    if (work->st.state == ACTOR_ENEMY_ANIM_BLEND) {
        pacedWalkBlendAnim(task);
        work->st.state = ACTOR_ENEMY_ANIM_TICK;
        return;
    }
    if (work->st.state == ACTOR_ENEMY_ANIM_RESET) {
        pacedWalkResetAnim(task);
        work->st.state = ACTOR_ENEMY_ANIM_TICK;
        return;
    }
    if (work->st.state == ACTOR_ENEMY_ANIM_TICK) {
        // The loop-end note ends cse's first block here, so the pause check
        // loads its own 1 instead of reusing the state test's.
        do {
        } while (0);
        animId = work->st.animId;
        if (animId == 4 && work->st.travel != 0) {
            _actorMovementStepForward(task->extra.tmd->coords, 0xC);
            work->st.travel--;
            if (work->st.travel == 0) {
                work->blendFrames = 0xA;
                work->st.animId   = 1;
            }
        }
        pacedWalkTickAnim(task);
        return;
    }
}
