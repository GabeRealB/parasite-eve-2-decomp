#include "main/random.h"

/* Part of the hopping enemy library; see hopping_enemy.h. */

/// Once the hold in `field_446` runs out, picks state 4 or 1 at random.
/// Before that, a player actor within 0xDAC moves the state machine to
/// state 3 and one within 0x1388 advances the sub-state.
void hopperLurkWait(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->work;
    s16              dist;

    if (work->field_446 < (s16)work->field_412++) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if ((gRandomLcgState >> 16) & 1) {
            Actor341700Work* w = (Actor341700Work*)arg0->work;

            w->field_420 = 4;
            w->field_422 = 0;
        } else {
            Actor341700Work* w = (Actor341700Work*)arg0->work;

            w->field_420 = 1;
            w->field_422 = 0;
        }
        return;
    }
    dist = work->field_43A;
    if (dist < 0xDAC) {
        Actor341700Work* next = (Actor341700Work*)arg0->work;

        next->field_420 = 3;
        next->field_422 = 0;
        return;
    }
    if (dist < 0x1388) {
        work->field_422++;
    }
}
