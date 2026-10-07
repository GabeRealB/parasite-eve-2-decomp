/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Begins the lurk alert animation and engages an idle scene battle.
///
/// Blends to clip 15 over eight normal-rate frames, advances the sub-state and
/// retains the frame counter and busy flag. Requires initialized Mad Chaser
/// work. Engaging battle does not acquire a reference or claim the shared alert.
static void _madChaserStartAlert(Task* task)
{
    enum {
        MAD_CHASER_LURK_ALERT_CLIP         = 15,
        MAD_CHASER_LURK_ALERT_BLEND_FRAMES = 8,
    };
    MadChaserWork* work = task->work;

    work->animBlendFrames = MAD_CHASER_LURK_ALERT_BLEND_FRAMES;
    work->animRate        = ANIMATION_RATE_ONE;
    work->animId          = MAD_CHASER_LURK_ALERT_CLIP;
    work->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
    work->subState        = work->subState + 1;
    sceneEngageBattle(1);
}
