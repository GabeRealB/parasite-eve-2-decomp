/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Advances the enemy's buildup-status reaction while maintaining its hold pose.
///
/// Requires live work, initialized animation storage, and an enemy with a
/// started buildup reaction, live parameters and grade in 0..3. Slot-1
/// boundary/jump/held status requests a four-frame normal-rate blend into
/// clip 11. Ticks buildup once each call, then selects combat walk at sub-state
/// zero when it expires. Does not clear the buildup flag or tick animation.
static void _madChaserStatusHold(Task* task)
{
    enum {
        MAD_CHASER_STATUS_HOLD_CLIP         = 11,
        MAD_CHASER_STATUS_HOLD_BLEND_FRAMES = 4,
    };
    MadChaserWork* requestWork;

    if (_madChaserAnimHasBoundaryStatus(task)) {
        requestWork                  = task->work;
        requestWork->animBlendFrames = MAD_CHASER_STATUS_HOLD_BLEND_FRAMES;
        requestWork->animRate        = ANIMATION_RATE_ONE;
        requestWork->animId          = MAD_CHASER_STATUS_HOLD_CLIP;
        requestWork->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
    }
    if (damageTickEnemyBuildup(task->spawnArg2.pointer) != 0) {
        _madChaserSetBehaviorState(task, MAD_CHASER_COMBAT_STATE_WALK);
    }
}
