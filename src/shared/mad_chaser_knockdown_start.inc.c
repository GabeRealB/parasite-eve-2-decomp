/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Saves the interrupted clip's stance and starts its knockdown pose.
///
/// Requires live work and a clip in 1..19, indexing the carrier's stance table
/// at clip minus one. Stance 1 is upright; all other values select the low pose.
/// Blends to clip 6 or 5 over six normal-rate frames and advances the sub-state.
static void _madChaserKnockdownStart(Task* task)
{
    enum {
        MAD_CHASER_KNOCKDOWN_UPRIGHT_STANCE = 1,
        MAD_CHASER_KNOCKDOWN_UPRIGHT_CLIP   = 6,
        MAD_CHASER_KNOCKDOWN_LOW_CLIP       = 5,
        MAD_CHASER_KNOCKDOWN_BLEND_FRAMES   = 6,
    };
    MadChaserWork* work;

    work               = task->work;
    work->stateScratch = gMadChaserAnimStance[work->animId - 1];
    if (work->stateScratch == MAD_CHASER_KNOCKDOWN_UPRIGHT_STANCE) {
        MadChaserWork* requestWork = task->work;

        requestWork->animBlendFrames = MAD_CHASER_KNOCKDOWN_BLEND_FRAMES;
        requestWork->animRate        = ANIMATION_RATE_ONE;
        requestWork->animId          = MAD_CHASER_KNOCKDOWN_UPRIGHT_CLIP;
        requestWork->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
    } else {
        MadChaserWork* requestWork = task->work;

        requestWork->animBlendFrames = MAD_CHASER_KNOCKDOWN_BLEND_FRAMES;
        requestWork->animRate        = ANIMATION_RATE_ONE;
        requestWork->animId          = MAD_CHASER_KNOCKDOWN_LOW_CLIP;
        requestWork->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
    }
    work->subState++;
}
