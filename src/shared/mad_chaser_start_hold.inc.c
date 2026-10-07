#include "main/random.h"

/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Starts a lurk idle hold with a random 96..159-tick threshold.
///
/// Requests the idle clip with a four-frame blend at normal rate and resets the
/// elapsed counter, then advances to the wait sub-state. Its elapsed test is
/// strict and precedes the increment. Advances the random sequence once; requires initialized
/// Mad Chaser work.
static void _madChaserLurkStartIdleHold(Task* task)
{
    enum {
        MAD_CHASER_IDLE_HOLD_CLIP         = 1,
        MAD_CHASER_IDLE_HOLD_BLEND_FRAMES = 4,
        MAD_CHASER_IDLE_HOLD_MIN_FRAMES   = 96,
        MAD_CHASER_IDLE_HOLD_RANDOM_MASK  = 63,
    };
    MadChaserWork* work;

    work                  = task->work;
    work->animBlendFrames = MAD_CHASER_IDLE_HOLD_BLEND_FRAMES;
    work->animRate        = ANIMATION_RATE_ONE;
    work->animId          = MAD_CHASER_IDLE_HOLD_CLIP;
    work->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
    gRandomLcgState       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->holdFrames      = ((gRandomLcgState >> 16) & MAD_CHASER_IDLE_HOLD_RANDOM_MASK) + MAD_CHASER_IDLE_HOLD_MIN_FRAMES;
    work->stateFrames     = 0;
    work->subState        = work->subState + 1;
}
