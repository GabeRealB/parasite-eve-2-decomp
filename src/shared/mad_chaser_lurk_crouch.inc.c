/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Requests the transition into the lurk look sequence once slot 1 reaches a control point.
///
/// Blends the transition clip over eight frames at normal rate, then advances
/// the look sequence's sub-state. Requires the initialized Mad Chaser work.
static void _madChaserLurkPrepareLook(Task* task)
{
    enum {
        MAD_CHASER_LOOK_TRANSITION_CLIP         = 13,
        MAD_CHASER_LOOK_TRANSITION_BLEND_FRAMES = 8,
    };
    MadChaserWork* work;
    MadChaserWork* requestWork;
    s32            hasBoundaryStatus;

    work = task->work;
    if ((work->slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (work->slots[1].status.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        hasBoundaryStatus = 1;
    } else {
        hasBoundaryStatus = 0;
    }
    if (hasBoundaryStatus) {
        requestWork                  = task->work;
        requestWork->animBlendFrames = MAD_CHASER_LOOK_TRANSITION_BLEND_FRAMES;
        requestWork->animRate        = ANIMATION_RATE_ONE;
        requestWork->animId          = MAD_CHASER_LOOK_TRANSITION_CLIP;
        requestWork->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
        work->subState++;
    }
}
