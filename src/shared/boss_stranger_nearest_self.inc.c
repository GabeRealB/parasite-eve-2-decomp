/* Part of the Boss Stranger library; see boss_stranger.h. */

/// Returns the patrol node nearest the walker: the squared XZ distance between
/// each node and the low halfwords of the walker coordinate's translation,
/// with the running best and the cursor staged in a scratch block. Same body
/// as the acropolis bridge room's `func_acropolis_bridge_8018450C`.
u8 bossStrangerNodeNearestSelf(BossStrangerWalker* work)
{
    BossStrangerNodeNearestSelfScratch* scan;

    scan = SCRATCH_STACK_RESERVE_BLOCK(BossStrangerNodeNearestSelfScratch);

    scan->bestDistSq = BOSS_STRANGER_NODE_DISTANCE_NONE;
    for (scan->node = 0; scan->node < work->nav->nodeCount; scan->node++) {
        scan->dx     = (u16)work->coord->coord.t[0] - work->nav->nodes[scan->node].x;
        scan->dz     = (u16)work->coord->coord.t[2] - work->nav->nodes[scan->node].z;
        scan->distSq = scan->dx * scan->dx + scan->dz * scan->dz;
        if (scan->distSq < scan->bestDistSq || scan->bestDistSq == BOSS_STRANGER_NODE_DISTANCE_NONE) {
            scan->bestDistSq = scan->distSq;
            scan->nearest    = scan->node;
        }
    }
    // Releasing only moves the cursor; the block is still intact for the read.
    SCRATCH_STACK_RELEASE_BLOCK(BossStrangerNodeNearestSelfScratch);
    return scan->nearest;
}
