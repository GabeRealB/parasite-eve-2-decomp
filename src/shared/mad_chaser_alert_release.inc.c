/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Releases this enemy's shared alert claim at an animation boundary.
///
/// Slot 1 reaching a boundary, control jump or settled pose releases only a
/// claim with this enemy's placement index. Starts clip 15 with an eight-frame
/// blend at normal rate and advances the alert sub-state; retains stateFrames.
static void _madChaserAlertRelease(Task* task)
{
    enum {
        MAD_CHASER_ALERT_RELEASE_ANIM = 15,
    };
    MadChaserWork* work;
    MadChaserWork* animationWork;

    work = task->work;
    if ((_madChaserAnimHasBoundaryStatus(task) << 0x10) != 0) {
        _madChaserSetAlertHold(task, 0);

        animationWork                  = task->work;
        animationWork->animBlendFrames = 8;
        animationWork->animRate        = ANIMATION_RATE_ONE;
        animationWork->animId          = MAD_CHASER_ALERT_RELEASE_ANIM;
        animationWork->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
        work->subState                 = work->subState + 1;
    }
}
