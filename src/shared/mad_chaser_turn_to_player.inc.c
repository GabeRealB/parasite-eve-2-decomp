/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Turns the heading `rotation.vy` by `step` toward the nearer player actor
/// (the offset in `toPlayer.vx` / `toPlayer.vz`), leaving it alone within 0x100.
void madChaserTurnToPlayer(Task* arg0, s32 step)
{
    MadChaserWork* work = (MadChaserWork*)arg0->work;
    SVECTOR        vec;
    s32            diff;
    u16            angle;
    s32            yaw;

    vec.vx = work->toPlayer.vx;
    vec.vy = 0;
    vec.vz = work->toPlayer.vz;
    VectorNormalSS(&vec, &vec);
    yaw   = ratan2(-vec.vx, -vec.vz);
    angle = work->rotation.vy;
    diff  = ((angle - yaw) << 20) >> 20;
    if (diff > 0x100) {
        work->rotation.vy = angle - step;
    } else if (diff < -0x100) {
        work->rotation.vy = angle + step;
    }
}
