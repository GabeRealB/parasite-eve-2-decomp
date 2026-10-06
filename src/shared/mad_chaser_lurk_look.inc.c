/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Holds for `holdFrames` frames, then moves to state 2. Over the last 0x30
/// frames the head yaw `spineYaw` eases back to zero; before that, while a
/// player actor is within 0xDAC and roughly ahead, it turns toward it and
/// after 16 such frames arms `gSceneCombatState` and moves to state 3, and
/// otherwise it sways between two fixed yaws by bit 6 of `frameCount`.
void madChaserLurkLookAround(Task* arg0)
{
    MadChaserWork* work = (MadChaserWork*)arg0->work;
    MadChaserWork* state;
    MadChaserWork* state2;
    s32            angle;
    s32            cur;
    s32            aim;

    if ((s16)++work->stateFrames > work->holdFrames) {
        state           = (MadChaserWork*)arg0->work;
        state->state    = 2;
        state->subState = 0;
        return;
    }
    if (work->holdFrames - 0x30 < (s16)work->stateFrames) {
        cur            = (u16)work->spineYaw;
        work->spineYaw = cur + ((s16)(-(cur * 16)) >> 9);
        return;
    }
    if (work->playerDist < 0xDAC && (aim = (u16)work->playerBearing, (aim < 0x3C0 || aim > 0xC40))) {
        angle          = (u16)work->spineYaw;
        work->spineYaw = angle + ((s16)((aim - angle) * 16) >> 6);
        if (++work->lookFrames >= 0x10) {
            sceneEngageBattle(1);
            state2           = (MadChaserWork*)arg0->work;
            state2->state    = 3;
            state2->subState = 0;
        }
    } else {
        // Both arms are spelled out: the cross-jumped tail leaves each its own
        // load of `spineYaw`, which a single update after an if/else lacks.
        if (!(((u16)work->frameCount >> 6) & 1)) {
            work->spineYaw = (u16)work->spineYaw + ((s16)(0x3800 - (u16)work->spineYaw * 16) >> 9);
        } else {
            work->spineYaw = (u16)work->spineYaw + ((s16)(-0x3800 - (u16)work->spineYaw * 16) >> 9);
        }
    }
}
