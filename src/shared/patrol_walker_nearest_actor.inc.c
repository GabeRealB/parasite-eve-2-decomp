/* Part of the patrol walker library; see patrol_walker.h. */

/// Scans the walker's patrol node table for the node nearest actor `actor` and
/// returns its index. Same scan as `patrolNodeNearestSelf`, but measured
/// from the translation of the actor config's matrix rather than from the
/// walker's own coordinate; the walker uses it with the player (entry 1) to
/// pick the node it retreats to.
u8 patrolNodeNearestActor(OverlayWalker* work, s32 actor)
{
    OverlayWalkerNearCfgScratch* block;
    u8*                          head;
    s16                          dz;

    head                     = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(u8) = head - 0x18;
    block                    = SCRATCH_STACK_CURSOR(OverlayWalkerNearCfgScratch);

    block->cfg  = &Player_Status + ((s16)actor - 1);
    block->best = -1;
    for (block->node = 0; block->node < work->nav->count; block->node++) {
        block->dx   = (u16)block->cfg->coordMtx->t[0] - work->nav->nodes[block->node].x;
        block->dy   = (u16)block->cfg->coordMtx->t[1] - work->nav->nodes[block->node].y;
        dz          = (u16)block->cfg->coordMtx->t[2] - work->nav->nodes[block->node].z;
        block->dz   = dz;
        block->dist = block->dx * block->dx + dz * dz;
        if (block->dist < block->best || block->best == -1) {
            block->best    = block->dist;
            block->nearest = block->node;
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(0x18);
    return block->nearest;
}
