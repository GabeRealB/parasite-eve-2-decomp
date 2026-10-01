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
    if ((work2->flags_EC.half & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (work2->flags_EC.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_420 = work->field_420 + 1;
    }
}
