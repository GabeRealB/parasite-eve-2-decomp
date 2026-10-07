#include "main/random.h"

/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Starts a look hold with a random 176..239-tick threshold at an animation control point.
///
/// Requests the look clip with a four-frame blend at normal rate, resets elapsed
/// and qualifying-look counters, and advances to the look update sub-state.
/// Advances the shared random sequence once. Requires initialized Mad Chaser work.
static void _madChaserLurkStartLookHold(Task* task)
{
    enum {
        MAD_CHASER_LOOK_HOLD_CLIP         = 14,
        MAD_CHASER_LOOK_HOLD_BLEND_FRAMES = 4,
        MAD_CHASER_LOOK_HOLD_MIN_FRAMES   = 176,
        MAD_CHASER_LOOK_HOLD_RANDOM_MASK  = 63,
    };
    MadChaserWork* work;
    MadChaserWork* requestWork;
    s32            hasBoundaryStatus;

    work = task->work;
    if ((work->slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (work->slots[1].status.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        hasBoundaryStatus = 1;
    } else {
        hasBoundaryStatus = 0;
    }
    if (hasBoundaryStatus) {
        requestWork                  = task->work;
        requestWork->animBlendFrames = MAD_CHASER_LOOK_HOLD_BLEND_FRAMES;
        requestWork->animRate        = ANIMATION_RATE_ONE;
        requestWork->animId          = MAD_CHASER_LOOK_HOLD_CLIP;
        requestWork->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
        gRandomLcgState              = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->stateFrames            = 0;
        work->lookFrames             = 0;
        work->holdFrames             = ((gRandomLcgState >> 16) & MAD_CHASER_LOOK_HOLD_RANDOM_MASK) + MAD_CHASER_LOOK_HOLD_MIN_FRAMES;
        work->subState++;
    }
}
