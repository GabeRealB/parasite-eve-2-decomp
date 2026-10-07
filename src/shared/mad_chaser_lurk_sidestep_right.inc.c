/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Applies the lurk right step along the heading's positive lateral axis.
///
/// distanceAtNormalRate is signed parent-coordinate units per normal-rate frame;
/// a positive value moves right. Uses the supplied heading plus a quarter turn
/// (4096 units per turn), with the task's live animation rate in sixteenths.
/// Scaling retains the signed low 20 product bits before division by 16 and
/// narrows the result to s16. Requires live work and root coordinates.
static __inline__ void _madChaserLurkMoveRight(Task* task, MadChaserWork* work, s32 distanceAtNormalRate)
{
    enum { MAD_CHASER_LURK_RIGHT_RATE_FRACTION_BITS = 4 };
    MadChaserWork* rateWork;
    s16            sideHeading;
    s16            stepDistance;

    sideHeading                           = work->rotation.vy + ACTOR_TRANSFORM_ANGLE_TURN / 4;
    rateWork                              = task->work;
    stepDistance                          = (rateWork->animRate * distanceAtNormalRate) << (16 - MAD_CHASER_LURK_RIGHT_RATE_FRACTION_BITS) >> 16;
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
