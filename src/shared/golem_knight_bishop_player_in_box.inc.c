/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Finds the first box region containing the player's world X/Z position.
///
/// `task` owns a live GOLEM work block and its borrowed room-region table.
/// Returns 1 and saves the index in `boxRegion` on a strict interior hit;
/// otherwise returns 0 and leaves the previous index unchanged.
static s32 _golemKnightBishopPlayerInBox(Task* task)
{
    GolemKnightBishopWork* work;
    s16                    regionCount;
    s32                    regionIndex;

    work        = task->work;
    regionCount = work->regionCount;
    for (regionIndex = 0; regionIndex < regionCount; regionIndex++) {
        if (work->regions[regionIndex].kind == GOLEM_KNIGHT_BISHOP_REGION_BOX) {
            if (GOLEM_KNIGHT_BISHOP_PLAYER_INSIDE_BOX(&work->regions[regionIndex], gPlayerStatus.coordMtx)) {
                work->boxRegion = regionIndex;
                return 1;
            }
        }
    }
    return 0;
}
