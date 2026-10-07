/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Side-steps right on frames 0x1D-0x29 and returns to the walk when the
/// animation ends.
void madChaserAlertSidestep(Task* arg0)
{
    MadChaserWork* work = (MadChaserWork*)arg0->work;
    s16            angle;
    s16            speed;

    if ((u16)(work->stateFrames++ - 0x1D) < 0xD) {
        speed                                 = _madChaserScaleByAnimRate(arg0, 0x1E);
        angle                                 = work->rotation.vy + 0x400;
        arg0->extra.tmd->coords->coord.t[0]  += ((rsin(angle) << 4) * speed) >> 0x10;
        arg0->extra.tmd->coords->coord.t[2]  += ((rcos(angle) << 4) * speed) >> 0x10;
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    if (_madChaserAnimHasBoundaryStatus(arg0)) {
        MadChaserWork* next;

        work->busy     = 0;
        next           = (MadChaserWork*)arg0->work;
        next->state    = 3;
        next->subState = 0;
    }
}
