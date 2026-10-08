/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Runs the handler of the work block's `sequence`, then the grab's release
/// timer unless the sequence is the grab, which runs it itself.
void golemKnightBishopRunSequence(Task* arg0)
{
    s16                    temp_v1;
    GolemKnightBishopWork* temp_s1;

    temp_s1 = arg0->work;
    temp_v1 = temp_s1->sequence;
    switch (temp_v1) {
        case GOLEM_KNIGHT_BISHOP_SEQUENCE_IDLE:
            _golemKnightBishopIdleSeq(arg0);
            break;
        case GOLEM_KNIGHT_BISHOP_SEQUENCE_GRAB:
            _golemKnightBishopGrabSeq(arg0);
            break;
        case GOLEM_KNIGHT_BISHOP_SEQUENCE_STRIKE:
            _golemKnightBishopStrikeSeq(arg0);
            break;
        case GOLEM_KNIGHT_BISHOP_SEQUENCE_BOX_APPROACH:
            _golemKnightBishopBoxApproachSeq(arg0);
            break;
        case GOLEM_KNIGHT_BISHOP_SEQUENCE_RECOVER:
            _golemKnightBishopRecoverSeq(arg0);
            break;
        case GOLEM_KNIGHT_BISHOP_SEQUENCE_LIGHT_FLINCH:
            _golemKnightBishopLightFlinchSeq(arg0);
            break;
        case GOLEM_KNIGHT_BISHOP_SEQUENCE_HEAVY_FLINCH:
            _golemKnightBishopHeavyFlinchSeq(arg0);
            break;
        case GOLEM_KNIGHT_BISHOP_SEQUENCE_KNEEL:
            _golemKnightBishopKnockdownSeq(arg0);
            break;
        case GOLEM_KNIGHT_BISHOP_SEQUENCE_KNEEL_HIT:
            _golemKnightBishopDownedHitSeq(arg0);
            break;
        case GOLEM_KNIGHT_BISHOP_SEQUENCE_COLLAPSE_DEATH:
            _golemKnightBishopCollapseDeathSeq(arg0);
            break;
        case GOLEM_KNIGHT_BISHOP_SEQUENCE_KNEEL_DEATH:
            _golemKnightBishopDownedDeathSeq(arg0);
            break;
        case GOLEM_KNIGHT_BISHOP_SEQUENCE_REGION_SCAN:
            _golemKnightBishopRegionScanSeq(arg0);
            break;
    }
    if (temp_s1->sequence != GOLEM_KNIGHT_BISHOP_SEQUENCE_GRAB) {
        _golemKnightBishopTickGrabRelease(arg0);
    }
}
