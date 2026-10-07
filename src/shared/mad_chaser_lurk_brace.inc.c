/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Starts the lurk alert's right sidestep after the preceding animation boundary.
///
/// A slot-1 boundary, jump or held pose resets the frame counter, marks the
/// move busy, blends to clip 4 over four normal-rate frames and advances the
/// sub-state. Requires initialized work; task and behavior state are retained.
static void _madChaserLurkAlertStartSidestep(Task* task)
{
    enum {
        MAD_CHASER_LURK_ALERT_SIDESTEP_CLIP         = 4,
        MAD_CHASER_LURK_ALERT_SIDESTEP_BLEND_FRAMES = 4,
    };
    MadChaserWork* work;
    MadChaserWork* requestWork;

    work = task->work;
    if (_madChaserAnimHasBoundaryStatusInline(task)) {
        work->stateFrames            = 0;
        work->busy                   = 1;
        requestWork                  = task->work;
        requestWork->animBlendFrames = MAD_CHASER_LURK_ALERT_SIDESTEP_BLEND_FRAMES;
        requestWork->animRate        = ANIMATION_RATE_ONE;
        requestWork->animId          = MAD_CHASER_LURK_ALERT_SIDESTEP_CLIP;
        requestWork->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
        work->subState++;
    }
}
