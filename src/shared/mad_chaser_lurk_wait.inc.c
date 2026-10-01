#include "main/random.h"

/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Once the hold in `field_446` runs out, picks state 4 or 1 at random.
/// Before that, a player actor within 0xDAC moves the state machine to
/// state 3 and one within 0x1388 advances the sub-state.
void madChaserLurkWait(Task* arg0)
{
    MadChaserWork* work = (MadChaserWork*)arg0->work;
    s16            dist;

    if (work->field_446 < (s16)work->field_412++) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if ((gRandomLcgState >> 16) & 1) {
            MadChaserWork* w = (MadChaserWork*)arg0->work;

            w->field_420 = 4;
            w->field_422 = 0;
        } else {
            MadChaserWork* w = (MadChaserWork*)arg0->work;

            w->field_420 = 1;
            w->field_422 = 0;
        }
        return;
    }
    dist = work->field_43A;
    if (dist < 0xDAC) {
        MadChaserWork* next = (MadChaserWork*)arg0->work;

        next->field_420 = 3;
        next->field_422 = 0;
        return;
    }
    if (dist < 0x1388) {
        work->field_422++;
    }
}
