/* Part of the hopping enemy library; see hopping_enemy.h. */

/// Ticks the animation and, once the hit flags are set, advances the state.
void hopperDeathWaitAnim(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;
    s32              cond;

    work = (Actor341700Work*)arg0->work;
    hopperTickAnim(arg0);
    work2 = (Actor341700Work*)arg0->work;
    if ((work2->flags_EC.half & 1) || (work2->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_420 = work->field_420 + 1;
    }
}
