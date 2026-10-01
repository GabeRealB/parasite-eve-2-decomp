/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Unless `madChaserJoinAlert` takes over, waits for the hit flags,
/// then marks the enemy busy, requests animation 4 and advances the
/// sub-state.
void madChaserLurkShiftBrace(Task* arg0)
{
    MadChaserWork* work;
    MadChaserWork* work2;
    MadChaserWork* work3;
    s32            cond;

    work = (MadChaserWork*)arg0->work;
    if ((madChaserJoinAlert(arg0) << 0x10) == 0) {
        work2 = (MadChaserWork*)arg0->work;
        if ((work2->flags_EC.half & ANIMATION_SLOT_REACHED_BOUNDARY) ||
            (work2->flags_EC.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            work->field_412  = 0;
            work->field_438  = 1;
            work3            = (MadChaserWork*)arg0->work;
            work3->field_426 = 4;
            work3->field_41C = 0x10;
            work3->field_418 = 4;
            work3->field_414 = 1;
            work->field_422  = work->field_422 + 1;
        }
    }
}
