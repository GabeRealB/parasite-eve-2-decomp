/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Moves the root laterally in its parent frame, scaled by animation rate.
///
/// distanceAtNormalRate is a signed displacement in parent-coordinate units.
/// Requires the task's live work and root. Scaling retains the signed low 20
/// product bits before division by 16; the displacement then narrows to s16.
static __inline__ void _madChaserLurkMoveRight(Task* task, MadChaserWork* work, s32 distanceAtNormalRate)
{
    MadChaserWork* rateWork;
    s16            sideHeading;
    s16            stepDistance;

    sideHeading                           = work->rotation.vy + ACTOR_TRANSFORM_ANGLE_TURN / 4;
    rateWork                              = task->work;
    stepDistance                          = (rateWork->animRate * distanceAtNormalRate) << 0xC >> 0x10;
    task->extra.tmd->coords->coord.t[0]  += ((rsin(sideHeading) << 4) * stepDistance) >> 0x10;
    task->extra.tmd->coords->coord.t[2]  += ((rcos(sideHeading) << 4) * stepDistance) >> 0x10;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Steps right in the lurk shift and starts its return animation.
///
/// Moves on previous counter values 29..41 inclusive, at 30 parent-coordinate
/// units per normal-rate frame along heading plus a quarter turn. A slot-1
/// boundary, jump or settled pose clears busy, starts clip 3 with an eight-frame
/// blend at normal rate, resets stateFrames and advances to the left step.
/// Requires live work and root coordinates; this step does not join shared alerts.
static void _madChaserLurkSidestepRight(Task* task)
{
    enum {
        MAD_CHASER_LURK_RIGHT_START_FRAME = 29,
        MAD_CHASER_LURK_RIGHT_FRAME_COUNT = 13,
        MAD_CHASER_LURK_SHIFT_LEFT_ANIM   = 3,
    };
    MadChaserWork* work;
    MadChaserWork* animationWork;
    s32            animationBoundary;

    work = task->work;
    if ((u16)(work->stateFrames++ - MAD_CHASER_LURK_RIGHT_START_FRAME) < MAD_CHASER_LURK_RIGHT_FRAME_COUNT) {
        _madChaserLurkMoveRight(task, work, 30);
    }

    animationWork = task->work;
    if ((animationWork->slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (animationWork->slots[1].status.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        animationBoundary = 1;
    } else {
        animationBoundary = 0;
    }
    if (animationBoundary) {
        work->busy = 0;

        animationWork                  = task->work;
        animationWork->animBlendFrames = 8;
        animationWork->animRate        = ANIMATION_RATE_ONE;
        animationWork->animId          = MAD_CHASER_LURK_SHIFT_LEFT_ANIM;
        animationWork->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
        work->stateFrames              = 0;
        work->subState++;
    }
}
