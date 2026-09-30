/* Part of the patrol walker library; see patrol_walker.h. */

/// Returns 1 when the walker is within field_5C*4 or 300 units (XZ) of its
/// current nav node, else 0.
s16 patrolArrived(OverlayWalker* walker)
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

    if (!overlayWalkerOutOfRange(d, walker->field_5C * 4) ||
        !overlayWalkerOutOfRange(d, 300)) {
        SCRATCH_STACK_RELEASE_BYTES(0x8);
        return 1;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x8);
    return 0;
}
