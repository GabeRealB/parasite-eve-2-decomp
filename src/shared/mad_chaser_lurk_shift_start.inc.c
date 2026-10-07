/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Starts the lurk shift's opening animation unless a shared alert takes over.
///
/// A claimed alert switches the task to combat. Otherwise starts clip 15 with
/// an eight-normal-frame blend and advances to the next shift sub-state.
/// Requires live work; busy and the frame counter are retained.
static void _madChaserLurkShiftStart(Task* task)
{
    enum {
        MAD_CHASER_LURK_SHIFT_START_ANIM = 15,
    };
    MadChaserWork* work;
    MadChaserWork* animationWork;

    work = task->work;
    if (_madChaserJoinAlert(task) == 0) {

        animationWork                  = task->work;
        animationWork->animBlendFrames = 8;
        animationWork->animRate        = ANIMATION_RATE_ONE;
        animationWork->animId          = MAD_CHASER_LURK_SHIFT_START_ANIM;
        animationWork->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
        work->subState                 = work->subState + 1;
    }
}
