/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Runs the sequence `field_6CC` names (0 to 0xB), then the vocal cue unless
/// the sequence is 1.
void golemKnightBishopRunSequence(Task* arg0)
{
    s16                    temp_v1;
    GolemKnightBishopWork* temp_s1;

    temp_s1 = arg0->work;
    temp_v1 = temp_s1->field_6CC;
    switch (temp_v1) {
        case 0:
            golemKnightBishopIdleSeq(arg0);
            break;
        case 1:
            golemKnightBishopGrabSeq(arg0);
            break;
        case 2:
            golemKnightBishopStrikeSeq(arg0);
            break;
        case 3:
            golemKnightBishopBoxApproachSeq(arg0);
            break;
        case 4:
            golemKnightBishopRecoverSeq(arg0);
            break;
        case 5:
            golemKnightBishopLightFlinchSeq(arg0);
            break;
        case 6:
            golemKnightBishopHeavyFlinchSeq(arg0);
            break;
        case 7:
            golemKnightBishopKneelSeq(arg0);
            break;
        case 8:
            golemKnightBishopKneelHitSeq(arg0);
            break;
        case 9:
            golemKnightBishopCollapseDeathSeq(arg0);
            break;
        case 10:
            golemKnightBishopKneelDeathSeq(arg0);
            break;
        case 11:
            golemKnightBishopBoxScanSeq(arg0);
            break;
    }
    if (temp_s1->field_6CC != 1) {
        golemKnightBishopHoldCueTimer(arg0);
    }
}
