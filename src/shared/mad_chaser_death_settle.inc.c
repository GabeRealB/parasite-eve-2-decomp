/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Requests a four-frame normal-rate blend into a death settle clip.
///
/// Requires initialized task-owned animation storage and a valid settle clip.
/// Reloads the live request work without advancing playback or behavior.
static __inline__ void _madChaserDeathBlendSettle(Task* task, s16 settleClip)
{
    enum { MAD_CHASER_DEATH_SETTLE_BLEND_FRAMES = 4 };
    MadChaserWork* requestWork = task->work;

    requestWork->animBlendFrames = MAD_CHASER_DEATH_SETTLE_BLEND_FRAMES;
    requestWork->animRate        = ANIMATION_RATE_ONE;
    requestWork->animId          = settleClip;
    requestWork->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
}

/// Credits the dying enemy's rewards and starts its stance-appropriate settle pose.
///
/// Requires a live enemy, model and initialized nine-slot animation/work storage
/// in ordinary-death behavior 1. The current clip must be in 1..19 for every
/// carrier's one-based settle mapping. Clip 8 chooses clip 5 before the first
/// leap, clip 6 afterwards; other clips use that mapping. Releases one battle
/// hold with rewards, requests a four-frame normal-rate blend, ticks slots 1..8
/// and advances to behavior 2. All task, model and clip storage remain live.
static void _madChaserDeathSettle(Task* task)
{
    enum {
        MAD_CHASER_DEATH_LEAP_CLIP           = 8,
        MAD_CHASER_DEATH_LOW_SETTLE_CLIP     = 5,
        MAD_CHASER_DEATH_UPRIGHT_SETTLE_CLIP = 6,
    };
    MadChaserWork* work;
    s16            currentClip;
    s16            settleClip;

    work = task->work;
    // Reward release precedes selection and the first tick of the death pose.
    sceneReleaseBattleRefWithRewards(task, 0);
    currentClip = work->animId;
    if (currentClip == MAD_CHASER_DEATH_LEAP_CLIP) {
        if (work->hasLeaped == 0) {
            _madChaserDeathBlendSettle(task, MAD_CHASER_DEATH_LOW_SETTLE_CLIP);
        } else {
            _madChaserDeathBlendSettle(task, MAD_CHASER_DEATH_UPRIGHT_SETTLE_CLIP);
        }
    } else {
        settleClip = gMadChaserSettleAnims[currentClip - 1];
        _madChaserDeathBlendSettle(task, settleClip);
    }
    _madChaserTickAnim(task);
    work->state++;
}
