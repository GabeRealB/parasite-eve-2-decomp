#include "main/random.h"

/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Once the hit flags are set, requests animation 0xE, clears the frame and
/// turn counters, draws a 0xB0..0xEF frame hold into `field_446` and
/// advances the sub-state.
void madChaserLurkRaise(Task* arg0)
{
    MadChaserWork* work;
    MadChaserWork* work2;
    s32            cond;

    work = (MadChaserWork*)arg0->work;
    if ((work->slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (work->slots[1].status.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work2            = (MadChaserWork*)arg0->work;
        work2->field_426 = 4;
        work2->field_41C = 0x10;
        work2->field_418 = 0xE;
        work2->field_414 = 1;
        gRandomLcgState  = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->field_412  = 0;
        work->field_42C  = 0;
        work->field_446  = ((gRandomLcgState >> 16) & 0x3F) + 0xB0;
        work->field_422++;
    }
}
