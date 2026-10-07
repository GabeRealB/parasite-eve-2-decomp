/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Starts the anchored dangle pose and advances to its sway.
///
/// Requires live model/work storage at the dangle entry sub-state. Anchors
/// part 6 at anchorPos through the frame callback and requests a normal-rate
/// cut to clip 7. The caller applies animation and the anchor after this step.
static void _madChaserDangleStart(Task* task)
{
    enum { MAD_CHASER_DANGLE_CLIP = 7 };
    MadChaserWork* work;
    MadChaserWork* requestWork;

    work                     = task->work;
    work->anchored           = 1;
    requestWork              = task->work;
    requestWork->animRate    = ANIMATION_RATE_ONE;
    requestWork->animId      = MAD_CHASER_DANGLE_CLIP;
    requestWork->animRequest = MAD_CHASER_ANIM_REQUEST_RESET;
    work->subState           = work->subState + 1;
}
