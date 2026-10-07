/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Starts the lurk rise with a four-frame blend into clip 15.
///
/// Requires initialized work/animation storage and the rise entry sub-state.
/// Requests normal-rate playback and advances to its boundary wait; the frame
/// callback applies the request and ticks the slots.
static void _madChaserLurkRiseStart(Task* task)
{
    enum {
        MAD_CHASER_LURK_RISE_CLIP         = 15,
        MAD_CHASER_LURK_RISE_BLEND_FRAMES = 4,
    };
    MadChaserWork* work = task->work;

    work->animBlendFrames = MAD_CHASER_LURK_RISE_BLEND_FRAMES;
    work->animRate        = ANIMATION_RATE_ONE;
    work->animId          = MAD_CHASER_LURK_RISE_CLIP;
    work->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
    work->subState        = work->subState + 1;
}
