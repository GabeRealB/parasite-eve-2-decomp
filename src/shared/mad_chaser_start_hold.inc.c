#include "main/random.h"

/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Requests animation 1, draws a 0x60..0x9F frame hold into `holdFrames`,
/// clears the frame counter and advances the sub-state.
void madChaserStartHold(Task* arg0)
{
    MadChaserWork* work;

    work                  = (MadChaserWork*)arg0->work;
    work->animBlendFrames = 4;
    work->animRate        = ANIMATION_RATE_ONE;
    work->animId          = 1;
    work->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
    /* Rolling the LCG through the global rather than an m2c temporary is what
     * hoists its `lw` above the field stores; see DECOMPILATION_LEARNINGS.md. */
    gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->holdFrames  = ((gRandomLcgState >> 16) & 0x3F) + 0x60;
    work->stateFrames = 0;
    work->subState    = work->subState + 1;
}
