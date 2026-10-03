/* Part of the Boss Stranger library; see boss_stranger.h. */

/// Scans the walker's patrol node table for the node nearest actor `actor` and
/// returns its index. Same scan as `bossStrangerNodeNearestSelf`, but measured
/// from the root coordinate of that player's status record rather than from the
/// walker's own coordinate; the walker uses it with the player (entry 1) to
/// pick the node it retreats to.
u8 bossStrangerNodeNearestActor(BossStrangerWalker* work, s32 actor)
{
    BossStrangerNodeNearestPlayerScratch* scan;

    scan = SCRATCH_STACK_RESERVE_BLOCK(BossStrangerNodeNearestPlayerScratch);

    scan->player     = &gPlayerStatus + ((s16)actor - 1);
    scan->bestDistSq = BOSS_STRANGER_NODE_DISTANCE_NONE;
    for (scan->node = 0; scan->node < work->nav->nodeCount; scan->node++) {
        scan->dx     = (u16)scan->player->coordMtx->t[0] - work->nav->nodes[scan->node].x;
        scan->dy     = (u16)scan->player->coordMtx->t[1] - work->nav->nodes[scan->node].y;
        scan->dz     = (u16)scan->player->coordMtx->t[2] - work->nav->nodes[scan->node].z;
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
