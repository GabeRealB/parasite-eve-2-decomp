/* Part of the pair walk library; see pair_walk.h. */

/// Step body of the actor's animation state machine. States 1 and 2 reseed the
/// animation slots (with and without `blendFrames`) and advance to state 3; state 3
/// walks the root coordinate 0x11 units per frame while clip 4 has `travel`
/// left, dropping back to clip 1 with argument 0xA when it runs out, then ticks
/// the slots.
void pairWalkUpdate(Task* task)
{
    PairWalkWork* work;
    s16           animId;

    work = task->work;
    if (work->st.state == ACTOR_ENEMY_ANIM_BLEND) {
        pairWalkReseedAnim(task);
        work->st.state = ACTOR_ENEMY_ANIM_TICK;
        return;
    }
    if (work->st.state == ACTOR_ENEMY_ANIM_RESET) {
        pairWalkResetAnim(task);
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
            _actorMovementStepForward(task->extra.tmd->coords, 0x11);
            work->st.travel--;
            if (work->st.travel == 0) {
                work->blendFrames = 0xA;
                work->st.animId   = 1;
            }
        }
        pairWalkTickAnim(task);
        return;
    }
}
