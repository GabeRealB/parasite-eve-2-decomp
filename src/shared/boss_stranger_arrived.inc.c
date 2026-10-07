/* Part of the Boss Stranger library; see boss_stranger.h. */

/// Returns 1 when the walker is within `speedTarget` * 4 or 300 units (XZ) of
/// its current nav node, else 0. The larger radius wins. `speedTarget` is
/// unsigned; 0xFFFE truncates to a negative radius when passed as an `s16`,
/// so the 300-unit test still decides.
s16 bossStrangerArrived(BossStrangerWalker* walker)
{
    SVECTOR* toNode;

    toNode = SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);

    // Stage the node's position, then subtract the walker's translation in
    // place. Only the low halfword of each coordinate takes part, and the
    // height is dropped because the test measures on the XZ plane.
    toNode->vx = walker->nav->nodes[walker->node].x;
    toNode->vy = walker->nav->nodes[walker->node].y;
    toNode->vz = walker->nav->nodes[walker->node].z;
    toNode->vx = toNode->vx - walker->coord->coord.t[0];
    toNode->vy = 0;
    toNode->vz = toNode->vz - walker->coord->coord.t[2];

    if (!actorOutsideRadius(toNode, walker->speedTarget * 4) ||
        !actorOutsideRadius(toNode, 300)) {
        SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
        return 1;
    }
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
    return 0;
}
