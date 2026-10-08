/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Saves the root position and advances it along the GOLEM's horizontal facing.
///
/// `forwardSpeed` is a signed world distance per frame; the root basis has
/// twelve fractional bits. While upright or starting to fall, a downward
/// floor-query displacement is added. Collision rejection can restore
/// `prevRootPos`.
static void _golemKnightBishopStepForward(Task* task)
{
    enum {
        GOLEM_KNIGHT_BISHOP_BASIS_FRACTION_BITS = 12,
        GOLEM_KNIGHT_BISHOP_FLOOR_QUERY_STEP    = 128,
        GOLEM_KNIGHT_BISHOP_KNOCKDOWN_FALLING   = 2,
    };
    GolemKnightBishopWork* work;
    GfxCoord*              root;

    root                 = &task->extra.tmd->coords[0];
    work                 = task->work;
    work->prevRootPos.vx = root->coord.t[0];
    work->prevRootPos.vy = root->coord.t[1];
    work->prevRootPos.vz = root->coord.t[2];
    root->coord.t[0]    += (root->coord.m[0][2] * work->forwardSpeed) >> GOLEM_KNIGHT_BISHOP_BASIS_FRACTION_BITS;
    if (work->knockdownStage < GOLEM_KNIGHT_BISHOP_KNOCKDOWN_FALLING) {
        root->coord.t[1] += GOLEM_KNIGHT_BISHOP_FLOOR_QUERY_STEP;
    }
    root->coord.t[2] += (root->coord.m[2][2] * work->forwardSpeed) >> GOLEM_KNIGHT_BISHOP_BASIS_FRACTION_BITS;
}
