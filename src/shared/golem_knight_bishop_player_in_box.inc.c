/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Reports whether the player stands inside one of the actor's box regions:
/// walks the `field_6FA` entries at `field_6B4` and, on the first
/// `GOLEM_KNIGHT_BISHOP_REGION_BOX` entry whose area holds the player's world
/// position (x between `minX` and `maxX`, z between `minZ` and `maxZ`), parks
/// its index in `field_708` and answers 1. Otherwise it answers 0.
s32 golemKnightBishopPlayerInBox(Task* arg0)
{
    GolemKnightBishopWork* work;
    s16                    count;
    s32                    i;

    work  = arg0->work;
    count = work->field_6FA;
    for (i = 0; i < count; i++) {
        if (work->field_6B4[i].kind == GOLEM_KNIGHT_BISHOP_REGION_BOX) {
            if ((work->field_6B4[i].minX < gPlayerStatus.coordMtx->t[0]) &&
                (gPlayerStatus.coordMtx->t[0] < work->field_6B4[i].maxX)) {
                if ((gPlayerStatus.coordMtx->t[2] < work->field_6B4[i].maxZ) &&
                    (work->field_6B4[i].minZ < gPlayerStatus.coordMtx->t[2])) {
                    work->field_708 = i;
                    return 1;
                }
            }
        }
    }
    return 0;
}
