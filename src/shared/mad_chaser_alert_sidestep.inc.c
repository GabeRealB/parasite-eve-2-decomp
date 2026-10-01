/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Side-steps right on frames 0x1D-0x29 and returns to the walk when the
/// animation ends.
void madChaserAlertSidestep(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->work;
    s16              angle;
    s16              speed;

    if ((u16)(work->field_412++ - 0x1D) < 0xD) {
        speed                                 = madChaserScaleBySpeed(arg0, 0x1E);
        angle                                 = work->field_7A + 0x400;
        arg0->extra.tmd->coords->coord.t[0]  += ((rsin(angle) << 4) * speed) >> 0x10;
        arg0->extra.tmd->coords->coord.t[2]  += ((rcos(angle) << 4) * speed) >> 0x10;
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    if (madChaserAnimEnded(arg0)) {
        Actor341700Work* next;

        work->field_438 = 0;
        next            = (Actor341700Work*)arg0->work;
        next->field_420 = 3;
        next->field_422 = 0;
    }
}
