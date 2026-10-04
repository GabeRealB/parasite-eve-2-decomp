/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Reports whether the player stands inside one of the room's box regions:
/// walks the `regionCount` entries at `regions` and, on the first
/// `GOLEM_KNIGHT_BISHOP_REGION_BOX` entry whose area holds the player's world
/// position (x between `minX` and `maxX`, z between `minZ` and `maxZ`), stores
/// its index in `boxRegion` and answers 1. Otherwise it answers 0.
s32 golemKnightBishopPlayerInBox(Task* arg0)
{
    GolemKnightBishopWork* work;
    s16                    count;
    s32                    i;

    work  = arg0->work;
    count = work->regionCount;
    for (i = 0; i < count; i++) {
        if (work->regions[i].kind == GOLEM_KNIGHT_BISHOP_REGION_BOX) {
            if ((work->regions[i].minX < gPlayerStatus.coordMtx->t[0]) &&
                (gPlayerStatus.coordMtx->t[0] < work->regions[i].maxX)) {
                if ((gPlayerStatus.coordMtx->t[2] < work->regions[i].maxZ) &&
                    (work->regions[i].minZ < gPlayerStatus.coordMtx->t[2])) {
                    work->boxRegion = i;
                    return 1;
                }
            }
        }
    }
    return 0;
}
