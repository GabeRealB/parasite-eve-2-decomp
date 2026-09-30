/* Part of the hopping enemy library; see hopping_enemy.h. */

/// When the recoil ends, returns to the walk (upright) or claims the alert hold
/// and goes to the alert state.
void hopperRecoilHeavyEnd(Task* arg0)
{
    Actor341700Work* work;
    s32              cond;

    work = (Actor341700Work*)arg0->work;
    if ((work->flags_EC.half & 1) || (work->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        if (work->field_44F == 1) {
            work            = (Actor341700Work*)arg0->work;
            work->field_420 = 3;
            work->field_422 = 0;
        } else {
            hopperSetAlertHold(arg0, 1);
            work            = (Actor341700Work*)arg0->work;
            work->field_420 = 5;
            work->field_422 = 0;
        }
    }
}
