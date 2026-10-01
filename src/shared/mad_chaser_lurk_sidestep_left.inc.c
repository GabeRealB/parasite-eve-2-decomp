/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Unless `madChaserJoinAlert` takes over, side-steps to the left on
/// frames 0x17..0x23 and, once the hit flags are set, returns the state
/// machine to state 0.
void madChaserLurkSidestepLeft(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;
    Actor341700Work* next;
    s32              cond;
    s16              angle;
    s16              speed;
    s32              scale;

    work = (Actor341700Work*)arg0->work;
    if ((madChaserJoinAlert(arg0) << 0x10) == 0) {
        if ((u16)(work->field_412++ - 0x17) < 0xD) {
            scale                                 = -0x1E;
            angle                                 = work->field_7A + 0x400;
            speed                                 = (((Actor341700Work*)arg0->work)->field_41C * scale) << 0xC >> 0x10;
            arg0->extra.tmd->coords->coord.t[0]  += ((rsin(angle) << 4) * speed) >> 0x10;
            arg0->extra.tmd->coords->coord.t[2]  += ((rcos(angle) << 4) * speed) >> 0x10;
            arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        }
        work2 = (Actor341700Work*)arg0->work;
        if ((work2->flags_EC.half & ANIMATION_SLOT_REACHED_BOUNDARY) ||
            (work2->flags_EC.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            work->field_438 = 0;
            next            = (Actor341700Work*)arg0->work;
            next->field_420 = 0;
            next->field_422 = 0;
        }
    }
}
