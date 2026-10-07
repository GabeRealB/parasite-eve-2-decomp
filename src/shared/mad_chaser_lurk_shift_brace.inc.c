/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Starts the lurk shift's right sidestep at the preceding animation boundary.
///
/// A claimed shared alert takes precedence and switches to combat. Otherwise a
/// slot-1 boundary, control jump or settled pose starts clip 4 with a four-frame
/// blend at normal rate, sets busy, resets stateFrames and advances the sub-state.
/// Requires live Mad Chaser work and loaded animation data.
static void _madChaserLurkShiftStartSidestep(Task* task)
{
    enum {
        MAD_CHASER_LURK_SHIFT_RIGHT_ANIM = 4,
    };
    MadChaserWork* work;
    MadChaserWork* statusWork;
    MadChaserWork* animationWork;
    s32            animationBoundary;

    work = task->work;
    if ((_madChaserJoinAlert(task) << 0x10) == 0) {

        statusWork = task->work;
        if ((statusWork->slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) ||
            (statusWork->slots[1].status.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
            animationBoundary = 1;
        } else {
            animationBoundary = 0;
        }
        if (animationBoundary) {
            work->stateFrames = 0;
            work->busy        = 1;

            animationWork                  = task->work;
            animationWork->animBlendFrames = 4;
            animationWork->animRate        = ANIMATION_RATE_ONE;
            animationWork->animId          = MAD_CHASER_LURK_SHIFT_RIGHT_ANIM;
            animationWork->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
            work->subState                 = work->subState + 1;
        }
    }
}
