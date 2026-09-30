/* Part of the hopping enemy library; see hopping_enemy.h. */

/// Unless `hopperJoinAlert` takes over, waits for the hit flags,
/// then marks the enemy busy, requests animation 4 and advances the
/// sub-state.
void hopperLurkShiftBrace(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;
    Actor341700Work* work3;
    s32              cond;

    work = (Actor341700Work*)arg0->work;
    if ((hopperJoinAlert(arg0) << 0x10) == 0) {
        work2 = (Actor341700Work*)arg0->work;
        if ((work2->flags_EC.half & 1) || (work2->flags_EC.word & 0x102)) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            work->field_412  = 0;
            work->field_438  = 1;
            work3            = (Actor341700Work*)arg0->work;
            work3->field_426 = 4;
            work3->field_41C = 0x10;
            work3->field_418 = 4;
            work3->field_414 = 1;
            work->field_422  = work->field_422 + 1;
        }
    }
}
