/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Requests the command-death settle pose, ticks it and advances the behavior.
///
/// Clip 8 settles to clip 5 before the first leap and clip 6 afterwards;
/// other clips use the carrier's one-based settle table. Requires initialized
/// animation storage and a current clip in 1..19. Blends over four normal-rate
/// frames. The battle reference and rewards are retained.
static void _madChaserDeathRequestSettle(Task* task)
{
    enum {
        MAD_CHASER_DEATH_LEAP_CLIP           = 8,
        MAD_CHASER_DEATH_LOW_SETTLE_CLIP     = 5,
        MAD_CHASER_DEATH_UPRIGHT_SETTLE_CLIP = 6,
        MAD_CHASER_DEATH_SETTLE_BLEND_FRAMES = 4,
    };
    MadChaserWork* work;
    s16            currentClip;
    s16            settleClip;

    work        = task->work;
    currentClip = work->animId;
    if (currentClip == MAD_CHASER_DEATH_LEAP_CLIP) {
        if (work->hasLeaped == 0) {
            work->animBlendFrames = MAD_CHASER_DEATH_SETTLE_BLEND_FRAMES;
            work->animRate        = ANIMATION_RATE_ONE;
            work->animId          = MAD_CHASER_DEATH_LOW_SETTLE_CLIP;
            work->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
        } else {
            work->animBlendFrames = MAD_CHASER_DEATH_SETTLE_BLEND_FRAMES;
            work->animRate        = ANIMATION_RATE_ONE;
            work->animId          = MAD_CHASER_DEATH_UPRIGHT_SETTLE_CLIP;
            work->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
        }
    } else {
        settleClip            = gMadChaserSettleAnims[currentClip - 1];
        work->animBlendFrames = MAD_CHASER_DEATH_SETTLE_BLEND_FRAMES;
        work->animRate        = ANIMATION_RATE_ONE;
        work->animId          = settleClip;
        work->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
    }
    _madChaserTickAnim(task);
    work->state++;
}
