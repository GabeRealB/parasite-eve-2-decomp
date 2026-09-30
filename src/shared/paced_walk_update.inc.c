/* Part of the paced walk library; see paced_walk.h. */

/// The actor's step body. States 1 and 2 reseed the animation slots (with and
/// without `animArg`) and advance to 3; state 3 walks the root coordinate 12
/// units per frame while the walk clip has `travel` left, switching to clip 1
/// with argument 0xA when it runs out, then ticks the slots.
void pacedWalkUpdate(Task* task)
{
    Actor160600Work* work;
    s16              animId;

    work = (Actor160600Work*)task->work;
    if (work->st.state == 1) {
        pacedWalkBlendAnim(task);
        work->st.state = 3;
        return;
    }
    if (work->st.state == 2) {
        pacedWalkResetAnim(task);
        work->st.state = 3;
        return;
    }
    if (work->st.state == 3) {
        // The loop-end note ends cse's first block here, so the pause check
        // loads its own 1 instead of reusing the state test's.
        do {
        } while (0);
        animId = work->st.animId;
        if (animId == 4 && work->st.travel != 0) {
            actorMoveForward(task->extra.tmd->coords, 0xC);
            work->st.travel = (u16)work->st.travel - 1;
            if (work->st.travel == 0) {
                work->animArg   = 0xA;
                work->st.animId = 1;
            }
        }
        pacedWalkTickAnim(task);
        return;
    }
}
