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
            golemKnightBishopIdleSeq(arg0);
            break;
        case GOLEM_KNIGHT_BISHOP_SEQUENCE_GRAB:
            golemKnightBishopGrabSeq(arg0);
            break;
        case GOLEM_KNIGHT_BISHOP_SEQUENCE_STRIKE:
            golemKnightBishopStrikeSeq(arg0);
            break;
        case GOLEM_KNIGHT_BISHOP_SEQUENCE_BOX_APPROACH:
            golemKnightBishopBoxApproachSeq(arg0);
            break;
        case GOLEM_KNIGHT_BISHOP_SEQUENCE_RECOVER:
            golemKnightBishopRecoverSeq(arg0);
            break;
        case GOLEM_KNIGHT_BISHOP_SEQUENCE_LIGHT_FLINCH:
            golemKnightBishopLightFlinchSeq(arg0);
            break;
        case GOLEM_KNIGHT_BISHOP_SEQUENCE_HEAVY_FLINCH:
            golemKnightBishopHeavyFlinchSeq(arg0);
            break;
        case GOLEM_KNIGHT_BISHOP_SEQUENCE_KNEEL:
            golemKnightBishopKneelSeq(arg0);
            break;
        case GOLEM_KNIGHT_BISHOP_SEQUENCE_KNEEL_HIT:
            golemKnightBishopKneelHitSeq(arg0);
            break;
        case GOLEM_KNIGHT_BISHOP_SEQUENCE_COLLAPSE_DEATH:
            golemKnightBishopCollapseDeathSeq(arg0);
            break;
        case GOLEM_KNIGHT_BISHOP_SEQUENCE_KNEEL_DEATH:
            golemKnightBishopKneelDeathSeq(arg0);
            break;
        case GOLEM_KNIGHT_BISHOP_SEQUENCE_REGION_SCAN:
            golemKnightBishopBoxScanSeq(arg0);
            break;
    }
    if (temp_s1->sequence != GOLEM_KNIGHT_BISHOP_SEQUENCE_GRAB) {
        golemKnightBishopHoldCueTimer(arg0);
    }
}
