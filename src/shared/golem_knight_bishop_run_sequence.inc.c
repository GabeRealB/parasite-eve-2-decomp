/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Dispatches one GOLEM sequence update and advances player-release bookkeeping.
///
/// `task` owns initialized GOLEM work/model data. Sequences 0..11 select the
/// shared behaviour handlers; other values dispatch none. The release update
/// tests the sequence after the handler may have changed it. A retained grab
/// sequence advances release only on its own release paths.
static void _golemKnightBishopRunSequence(Task* task)
{
    GolemKnightBishopWork* work;

    work = task->work;
    switch (work->sequence) {
        case GOLEM_KNIGHT_BISHOP_SEQUENCE_IDLE:
            _golemKnightBishopIdleSeq(task);
            break;
        case GOLEM_KNIGHT_BISHOP_SEQUENCE_GRAB:
            _golemKnightBishopGrabSeq(task);
            break;
        case GOLEM_KNIGHT_BISHOP_SEQUENCE_STRIKE:
            _golemKnightBishopStrikeSeq(task);
            break;
        case GOLEM_KNIGHT_BISHOP_SEQUENCE_BOX_APPROACH:
            _golemKnightBishopBoxApproachSeq(task);
            break;
        case GOLEM_KNIGHT_BISHOP_SEQUENCE_RECOVER:
            _golemKnightBishopRecoverSeq(task);
            break;
        case GOLEM_KNIGHT_BISHOP_SEQUENCE_LIGHT_FLINCH:
            _golemKnightBishopLightFlinchSeq(task);
            break;
        case GOLEM_KNIGHT_BISHOP_SEQUENCE_HEAVY_FLINCH:
            _golemKnightBishopHeavyFlinchSeq(task);
            break;
        case GOLEM_KNIGHT_BISHOP_SEQUENCE_KNEEL:
            _golemKnightBishopKnockdownSeq(task);
            break;
        case GOLEM_KNIGHT_BISHOP_SEQUENCE_KNEEL_HIT:
            _golemKnightBishopDownedHitSeq(task);
            break;
        case GOLEM_KNIGHT_BISHOP_SEQUENCE_COLLAPSE_DEATH:
            _golemKnightBishopCollapseDeathSeq(task);
            break;
        case GOLEM_KNIGHT_BISHOP_SEQUENCE_KNEEL_DEATH:
            _golemKnightBishopDownedDeathSeq(task);
            break;
        case GOLEM_KNIGHT_BISHOP_SEQUENCE_REGION_SCAN:
            _golemKnightBishopRegionScanSeq(task);
            break;
    }
    // Re-read the sequence: release may begin or end during the handler above.
    if (work->sequence != GOLEM_KNIGHT_BISHOP_SEQUENCE_GRAB) {
        _golemKnightBishopTickGrabRelease(task);
    }
}
