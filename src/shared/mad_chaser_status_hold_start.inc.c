/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Starts the buildup-status hold with an eight-frame blend into clip 12.
///
/// Requires live animation/work storage and the status entry sub-state. The
/// enemy's buildup reaction must already be started. Requests normal-rate
/// playback and advances to the hold; animation ticking belongs to the caller.
static void _madChaserStatusHoldStart(Task* task)
{
    enum {
        MAD_CHASER_STATUS_ENTRY_CLIP         = 12,
        MAD_CHASER_STATUS_ENTRY_BLEND_FRAMES = 8,
    };
    MadChaserWork* work = task->work;

    work->animBlendFrames = MAD_CHASER_STATUS_ENTRY_BLEND_FRAMES;
    work->animRate        = ANIMATION_RATE_ONE;
    work->animId          = MAD_CHASER_STATUS_ENTRY_CLIP;
    work->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
    work->subState        = work->subState + 1;
}
