#include "main/random.h"

/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Once the hold in `holdFrames` runs out, picks state 4 or 1 at random.
/// Before that, a player actor within 0xDAC moves the state machine to
/// state 3 and one within 0x1388 advances the sub-state.
void madChaserLurkWait(Task* arg0)
{
    MadChaserWork* work = (MadChaserWork*)arg0->work;
    s16            dist;

    if (work->holdFrames < (s16)work->stateFrames++) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if ((gRandomLcgState >> 16) & 1) {
            MadChaserWork* w = (MadChaserWork*)arg0->work;

            w->state    = 4;
            w->subState = 0;
        } else {
            MadChaserWork* w = (MadChaserWork*)arg0->work;

            w->state    = 1;
            w->subState = 0;
        }
        return;
    }
    dist = work->playerDist;
    if (dist < 0xDAC) {
        MadChaserWork* next = (MadChaserWork*)arg0->work;

        next->state    = 3;
        next->subState = 0;
        return;
    }
    if (dist < 0x1388) {
        work->subState++;
    }
}
