/* Part of the hopping enemy library; see hopping_enemy.h. */

/// Unless `hopperJoinAlert` takes over, requests animation 0xF
/// and advances the sub-state.
void hopperLurkShiftStart(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;

    work = (Actor341700Work*)arg0->work;
    if (hopperJoinAlert(arg0) == 0) {
        work2            = (Actor341700Work*)arg0->work;
        work2->field_426 = 8;
        work2->field_41C = 0x10;
        work2->field_418 = 0xF;
        work2->field_414 = 1;
        work->field_422  = work->field_422 + 1;
    }
}
