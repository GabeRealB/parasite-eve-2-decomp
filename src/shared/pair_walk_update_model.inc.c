/* Part of the pair walk library; see pair_walk.h. */

/// The enemy's animation state machine, run by its spawn and per-frame
/// handlers. States 1 and 2 start the clip in `animId` through
/// `pairWalkReseedAnim` or `pairWalkResetAnim` and advance to
/// state 3. State 3 walks the model 12 units a frame while the walk clip (4)
/// has `travel` left, dropping back to clip 1 with reset argument 0xA when it
/// runs out, then ticks the slots.
void pairWalkUpdate(Task* task)
{
    Actor150400Work* work;
    s16              animId;

    work = (Actor150400Work*)task->work;
    if (work->st.state == 1) {
        pairWalkReseedAnim(task);
        work->st.state = 3;
        return;
    }
    if (work->st.state == 2) {
        pairWalkResetAnim(task);
        work->st.state = 3;
        return;
    }
    if (work->st.state == 3) {
        do {
        } while (0);
        animId = work->st.animId;
        if (animId == 4 && work->st.travel != 0) {
            actorMoveModelForward(task, 0xC);
            work->st.travel = (u16)work->st.travel - 1;
            if (work->st.travel == 0) {
                work->animArg   = 0xA;
                work->st.animId = 1;
            }
        }
        pairWalkTickAnim(task);
        return;
    }
}
