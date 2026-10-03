/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Keeps turning and stepping toward the player until the walk cycle ends, then
/// goes to the leap state.
void madChaserWalkFinish(Task* arg0)
{
    MadChaserWork* work = (MadChaserWork*)arg0->work;
    s16            angle;
    s16            speed;

    madChaserTurnToPlayer(arg0, 0x10);
    speed                                 = madChaserScaleBySpeed(arg0, -0x10);
    angle                                 = work->rotation.vy;
    arg0->extra.tmd->coords->coord.t[0]  += ((rsin(angle) << 4) * speed) >> 0x10;
    arg0->extra.tmd->coords->coord.t[2]  += ((rcos(angle) << 4) * speed) >> 0x10;
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (madChaserAnimEnded(arg0)) {
        MadChaserWork* next = (MadChaserWork*)arg0->work;

        next->state    = 4;
        next->subState = 0;
    }
}
