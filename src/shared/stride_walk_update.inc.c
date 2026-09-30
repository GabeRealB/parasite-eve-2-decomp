/* Part of the stride walk library; see stride_walk.h. */

/// Step body of the actor's animation state machine. States 1 and 2 reseed the
/// animation slots (with and without `animArg`) and advance to state 3; state 3
/// walks the root coordinate 0x1E units per frame while clip 4 has `travel`
/// left, dropping back to clip 1 with argument 0xA and to state 1 when it runs out, then ticks the slots.
void strideWalkUpdate(Task* task)
{
    Actor161500Work* work;
    s16              animId;

    work = (Actor161500Work*)task->work;
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
        do {
        } while (0);
        animId = work->st.animId;
        if (animId == 4 && work->st.travel != 0) {
            actorMoveForward(task->extra.tmd->coords, 0x1E);
            work->st.travel = (u16)work->st.travel - 1;
            if (work->st.travel == 0) {
                work->st.state  = 1;
                work->animArg   = 0xA;
                work->st.animId = 1;
            }
        }
        pacedWalkTickAnim(task);
        return;
    }
}
