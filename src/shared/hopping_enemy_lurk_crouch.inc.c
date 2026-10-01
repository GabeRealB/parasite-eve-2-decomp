/* Part of the hopping enemy library; see hopping_enemy.h. */

/// Once the hit flags are set, requests animation 0xD and advances the
/// sub-state.
void hopperLurkCrouch(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;
    s32              cond;

    work = (Actor341700Work*)arg0->work;
    if ((work->flags_EC.half & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (work->flags_EC.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work2            = (Actor341700Work*)arg0->work;
        work2->field_426 = 8;
        work2->field_41C = 0x10;
        work2->field_418 = 0xD;
        work2->field_414 = 1;
        work->field_422++;
    }
}
