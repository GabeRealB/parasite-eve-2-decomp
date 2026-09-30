/* Part of the patrol walker library; see patrol_walker.h. */

/// Returns the patrol node nearest the walker: the squared XZ distance between
/// each node and the low halfwords of the walker coordinate's translation,
/// with the running best and the cursor staged in a scratch block. Same body
/// as the acropolis bridge room's `func_acropolis_bridge_8018450C`.
u8 patrolNodeNearestSelf(OverlayWalker* work)
{
    OverlayWalkerNearScratch* block;
    u8*                       head;
    s16                       dz;

    head                     = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(u8) = head - 0x14;
    block                    = SCRATCH_STACK_CURSOR(OverlayWalkerNearScratch);

    block->best = -1;
    for (block->node = 0; block->node < work->nav->count; block->node++) {
        block->dx   = (u16)work->coord->coord.t[0] - work->nav->nodes[block->node].x;
        dz          = (u16)work->coord->coord.t[2] - work->nav->nodes[block->node].z;
        block->dz   = dz;
        block->dist = block->dx * block->dx + dz * dz;
        if (block->dist < block->best || block->best == -1) {
            block->best    = block->dist;
            block->nearest = block->node;
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(0x14);
    return block->nearest;
}
