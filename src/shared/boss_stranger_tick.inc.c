/* Part of the Boss Stranger library; see boss_stranger.h. */

/// Advances the walker once and invalidates its composed coordinate.
///
/// Opens an uninitialized goal frame, runs the steering and movement step,
/// marks the coordinate dirty and releases one frame from the resulting cursor.
/// Requires `_bossStrangerStep`'s live-input and scratch contracts. Idle retains
/// stale goal data; close-in also does so and, on a non-arriving tick, leaves
/// the cursor four bytes below its entry value even after the frame release.
/// The known carriers select idle, chase or patrol. No pointer is retained.
static void _bossStrangerTick(BossStrangerWalker* walker)
{
    BossStrangerTickScratch* scratchEnd;
    BossStrangerTickScratch* frame;

    scratchEnd = SCRATCH_STACK_CURSOR(BossStrangerTickScratch);
    frame      = SCRATCH_STACK_RESERVE_BLOCK(BossStrangerTickScratch);
    _bossStrangerStep(walker, scratchEnd, frame);
    walker->coord->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BLOCK(BossStrangerTickScratch);
}
