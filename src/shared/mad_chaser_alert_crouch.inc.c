/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Starts the alert's right sidestep after the preceding animation reaches a boundary.
///
/// A boundary, control jump or settled slot 1 starts clip 4 with a four-frame
/// blend at normal rate, marks the move busy and resets its frame counter.
/// Requires live work and a nine-part model; advances the alert sub-state once.
static void _madChaserAlertStartSidestep(Task* task)
{
    enum {
        MAD_CHASER_ALERT_SIDESTEP_ANIM = 4,
    };
    MadChaserWork* work;
    MadChaserWork* animationWork;

    work = task->work;
    if ((_madChaserAnimHasBoundaryStatus(task) << 0x10) != 0) {
        work->busy        = 1;
        work->stateFrames = 0;

        animationWork                  = task->work;
        animationWork->animBlendFrames = 4;
        animationWork->animRate        = ANIMATION_RATE_ONE;
        animationWork->animId          = MAD_CHASER_ALERT_SIDESTEP_ANIM;
        animationWork->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
        work->subState                 = work->subState + 1;
    }
}
