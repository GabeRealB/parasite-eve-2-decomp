/* Part of the Boss Stranger library; see boss_stranger.h. */

/// Per-tick walker step. Opens a scratch frame, runs `bossStrangerStep`
/// (chase, close-in or patrol, then the speed ramp and optional ground and
/// avoidance steps) and marks the coordinate dirty. Same body as the
/// acropolis bridge room's `func_acropolis_bridge_8018532C`.
void bossStrangerTick(BossStrangerWalker* walker)
{
    u8*                       head;
    OverlayWalkerTickScratch* block;

    head                     = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(u8) = head - 0x28;
    block                    = SCRATCH_STACK_CURSOR(OverlayWalkerTickScratch);
    bossStrangerStep(walker, head, block);
    walker->coord->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BYTES(0x28);
}
