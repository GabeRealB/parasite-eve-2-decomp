/* Part of the Boss Stranger library; see boss_stranger.h. */

/// Per-tick walker step. Opens a scratch frame, runs `bossStrangerStep`
/// (chase, close-in or patrol, then the speed ramp and optional ground and
/// avoidance steps) and marks the coordinate dirty. Same body as the
/// acropolis bridge room's `func_acropolis_bridge_8018532C`.
void bossStrangerTick(BossStrangerWalker* walker)
{
    BossStrangerTickScratch* head;
    BossStrangerTickScratch* block;

    head                                          = SCRATCH_STACK_CURSOR(BossStrangerTickScratch);
    SCRATCH_STACK_CURSOR(BossStrangerTickScratch) = head - 1;
    block                                         = SCRATCH_STACK_CURSOR(BossStrangerTickScratch);
    bossStrangerStep(walker, head, block);
    walker->coord->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BLOCK(BossStrangerTickScratch);
}
