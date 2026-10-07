/* Part of the Boss Stranger library; see boss_stranger.h. */

/// Returns the navigation node nearest the player, measured on the XZ plane.
///
/// `playerId` is a one-based resident player-status selector; every known
/// caller passes 1, the only record whose storage is established. The selected
/// player's root matrix must be live in the navigation nodes' coordinate frame.
/// Requires `nav->nodeCount` in 1..255 and that many readable node entries.
/// Axis differences wrap to signed halfwords; squared distances are compared
/// as unsigned words. Ties retain the first node in table order. An empty table
/// returns an unwritten scratch byte and is outside the caller contract.
/// Borrows the walker read-only and one initialized scratch block for the call,
/// restoring the cursor before reading the result without another reservation.
static u8 _bossStrangerNodeNearestPlayer(const BossStrangerWalker* walker, s16 playerId)
{
    BossStrangerNodeNearestPlayerScratch* scan;

    scan = SCRATCH_STACK_RESERVE_BLOCK(BossStrangerNodeNearestPlayerScratch);

    scan->player     = &gPlayerStatus + (playerId - 1);
    scan->bestDistSq = BOSS_STRANGER_NODE_DISTANCE_NONE;
    // Bound the translation to its low halfword before subtracting a node.
    for (scan->node = 0; scan->node < walker->nav->nodeCount; scan->node++) {
        scan->dx     = (u16)scan->player->coordMtx->t[0] - walker->nav->nodes[scan->node].x;
        scan->dy     = (u16)scan->player->coordMtx->t[1] - walker->nav->nodes[scan->node].y;
        scan->dz     = (u16)scan->player->coordMtx->t[2] - walker->nav->nodes[scan->node].z;
        scan->distSq = scan->dx * scan->dx + scan->dz * scan->dz;
        if (scan->distSq < scan->bestDistSq || scan->bestDistSq == BOSS_STRANGER_NODE_DISTANCE_NONE) {
            scan->bestDistSq = scan->distSq;
            scan->nearest    = scan->node;
        }
    }
    // Releasing only moves the cursor; the block is still intact for the read.
    SCRATCH_STACK_RELEASE_BLOCK(BossStrangerNodeNearestPlayerScratch);
    return scan->nearest;
}
