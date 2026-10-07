/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Keeps turning and stepping toward the player until the walk slot reports a boundary, jump or hold, then
/// goes to the leap state.
void madChaserWalkFinish(Task* arg0)
{
    MadChaserWork* work = (MadChaserWork*)arg0->work;
    s16            angle;
    s16            speed;

    _madChaserTurnToPlayer(arg0, 0x10);
    speed                                 = _madChaserScaleByAnimRate(arg0, -0x10);
    angle                                 = work->rotation.vy;
    arg0->extra.tmd->coords->coord.t[0]  += ((rsin(angle) << 4) * speed) >> 0x10;
    arg0->extra.tmd->coords->coord.t[2]  += ((rcos(angle) << 4) * speed) >> 0x10;
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (_madChaserAnimHasBoundaryStatus(arg0)) {
        MadChaserWork* next = (MadChaserWork*)arg0->work;

        next->state    = 4;
        next->subState = 0;
    }
}
