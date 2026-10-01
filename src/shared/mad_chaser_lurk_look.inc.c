/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Holds for `field_446` frames, then moves to state 2. Over the last 0x30
/// frames the head yaw `field_424` eases back to zero; before that, while a
/// player actor is within 0xDAC and roughly ahead, it turns toward it and
/// after 16 such frames arms `gSceneCombatState` and moves to state 3, and
/// otherwise it sways between two fixed yaws by bit 6 of `field_442`.
void madChaserLurkLookAround(Task* arg0)
{
    MadChaserWork* work = (MadChaserWork*)arg0->work;
    MadChaserWork* state;
    MadChaserWork* state2;
    s32            angle;
    s32            cur;
    s32            aim;

    if ((s16)++work->field_412 > work->field_446) {
        state            = (MadChaserWork*)arg0->work;
        state->field_420 = 2;
        state->field_422 = 0;
        return;
    }
    if (work->field_446 - 0x30 < (s16)work->field_412) {
        cur             = (u16)work->field_424;
        work->field_424 = cur + ((s16)(-(cur * 16)) >> 9);
        return;
    }
    if (work->field_43A < 0xDAC && (aim = (u16)work->field_444, (aim < 0x3C0 || aim > 0xC40))) {
        angle           = (u16)work->field_424;
        work->field_424 = angle + ((s16)((aim - angle) * 16) >> 6);
        if (++work->field_42C >= 0x10) {
            Gp_ArmStateF0(1);
            state2            = (MadChaserWork*)arg0->work;
            state2->field_420 = 3;
            state2->field_422 = 0;
        }
    } else {
        // Both arms are spelled out: the cross-jumped tail leaves each its own
        // load of `field_424`, which a single update after an if/else lacks.
        if (!(((u16)work->field_442 >> 6) & 1)) {
            work->field_424 = (u16)work->field_424 + ((s16)(0x3800 - (u16)work->field_424 * 16) >> 9);
        } else {
            work->field_424 = (u16)work->field_424 + ((s16)(-0x3800 - (u16)work->field_424 * 16) >> 9);
        }
    }
}
