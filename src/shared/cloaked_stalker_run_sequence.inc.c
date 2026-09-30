/* Part of the cloaked stalker library; see cloaked_stalker.h. */

/// Runs the sequence `field_6CC` names (0 to 0xB), then the vocal cue unless
/// the sequence is 1.
void stalkerRunSequence(Task* arg0)
{
    s16              temp_v1;
    Actor402200Work* temp_s1;

    temp_s1 = arg0->work;
    temp_v1 = temp_s1->field_6CC;
    switch (temp_v1) {
        case 0:
            stalkerIdleSeq(arg0);
            break;
        case 1:
            stalkerGrabSeq(arg0);
            break;
        case 2:
            stalkerStrikeSeq(arg0);
            break;
        case 3:
            stalkerBoxApproachSeq(arg0);
            break;
        case 4:
            stalkerRecoverSeq(arg0);
            break;
        case 5:
            stalkerLightFlinchSeq(arg0);
            break;
        case 6:
            stalkerHeavyFlinchSeq(arg0);
            break;
        case 7:
            stalkerKneelSeq(arg0);
            break;
        case 8:
            stalkerKneelHitSeq(arg0);
            break;
        case 9:
            stalkerCollapseDeathSeq(arg0);
            break;
        case 10:
            stalkerKneelDeathSeq(arg0);
            break;
        case 11:
            stalkerBoxScanSeq(arg0);
            break;
    }
    if (temp_s1->field_6CC != 1) {
        stalkerHoldCueTimer(arg0);
    }
}
