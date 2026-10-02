/* Part of the Boss Stranger library; see boss_stranger.h. */

/// Returns 1 when the walker is within `speedTarget` * 4 or 300 units (XZ) of
/// its current nav node, else 0. The larger radius wins. `speedTarget` is
/// unsigned; 0xFFFE truncates to a negative radius when passed as an `s16`,
/// so the 300-unit test still decides.
s16 bossStrangerArrived(BossStrangerWalker* walker)
{
    OverlayWalkerArrivalDelta* d;
    u8*                        head;

    head                     = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(u8) = head - 0x8;
    d                        = (OverlayWalkerArrivalDelta*)(head - 0x8);

    d->x = walker->nav->nodes[walker->node].x;
    d->y = walker->nav->nodes[walker->node].y;
    d->z = walker->nav->nodes[walker->node].z;
    d->x = d->x - (u16)walker->coord->coord.t[0];
    d->y = 0;
    d->z = d->z - (u16)walker->coord->coord.t[2];

    if (!overlayWalkerOutOfRange(d, walker->speedTarget * 4) ||
        !overlayWalkerOutOfRange(d, 300)) {
        SCRATCH_STACK_RELEASE_BYTES(0x8);
        return 1;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x8);
    return 0;
}
