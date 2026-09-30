/* Part of the hopping enemy library; see hopping_enemy.h. */

/// Side-steps to the right of the heading at a speed scaled by `field_41C`
/// on frames 0x1D..0x29; once the hit flags are set, moves the task and the
/// state machine to state 3.
void hopperLurkSidestepToCombat(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;
    s32              cond;
    s16              angle;
    s16              speed;
    s32              scale;

    work = (Actor341700Work*)arg0->work;
    if ((u16)(work->field_412++ - 0x1D) < 0xD) {
        scale                                 = 0x1E;
        angle                                 = work->field_7A + 0x400;
        speed                                 = (((Actor341700Work*)arg0->work)->field_41C * scale) << 0xC >> 0x10;
        arg0->extra.tmd->coords->coord.t[0]  += ((rsin(angle) << 4) * speed) >> 0x10;
        arg0->extra.tmd->coords->coord.t[2]  += ((rcos(angle) << 4) * speed) >> 0x10;
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    work2 = (Actor341700Work*)arg0->work;
    if ((work2->flags_EC.half & 1) || (work2->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_438 = 0;
        hopperEnter_state(arg0, 3);
        hopperSet_state(arg0, 3);
    }
}
