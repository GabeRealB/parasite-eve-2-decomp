/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Applies the lurk left step along the heading's positive lateral axis.
///
/// distanceAtNormalRate is signed parent-coordinate units per normal-rate frame;
/// a negative value moves left. Uses the supplied heading plus a quarter turn
/// (4096 units per turn), with the task's live animation rate in sixteenths.
/// Scaling retains the signed low 20 product bits before division by 16 and
/// narrows the result to s16. Requires live work and root coordinates.
static __inline__ void _madChaserLurkMoveLeft(Task* task, MadChaserWork* work, s32 distanceAtNormalRate)
{
    enum { MAD_CHASER_LURK_LEFT_RATE_FRACTION_BITS = 4 };
    MadChaserWork* rateWork;
    s16            sideHeading;
    s16            stepDistance;

    sideHeading                           = work->rotation.vy + ACTOR_TRANSFORM_ANGLE_TURN / 4;
    rateWork                              = task->work;
    stepDistance                          = (rateWork->animRate * distanceAtNormalRate) << (16 - MAD_CHASER_LURK_LEFT_RATE_FRACTION_BITS) >> 16;
    task->extra.tmd->coords->coord.t[0]  += ((rsin(sideHeading) << 4) * stepDistance) >> 0x10;
    task->extra.tmd->coords->coord.t[2]  += ((rcos(sideHeading) << 4) * stepDistance) >> 0x10;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Steps left to finish the lurk shift and returns to lurk idle.
///
/// A claimed shared alert takes precedence. Otherwise moves on previous counter
/// values 23..35 inclusive, at -30 parent-coordinate units per normal-rate frame
/// along heading plus a quarter turn. A slot-1 boundary, jump or settled pose
/// clears busy and resets the lurk behavior and sub-state to zero.
/// Requires live work and root coordinates; the frame counter wraps as u16.
static void _madChaserLurkSidestepLeft(Task* task)
{
    enum {
        MAD_CHASER_LURK_LEFT_START_FRAME = 23,
        MAD_CHASER_LURK_LEFT_FRAME_COUNT = 13,
    };
    MadChaserWork* work;
    MadChaserWork* animationWork;
    MadChaserWork* nextWork;
    s32            animationBoundary;

    work = task->work;
    if ((_madChaserJoinAlert(task) << 0x10) == 0) {
        if ((u16)(work->stateFrames++ - MAD_CHASER_LURK_LEFT_START_FRAME) < MAD_CHASER_LURK_LEFT_FRAME_COUNT) {
            _madChaserLurkMoveLeft(task, work, -30);
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

            nextWork           = task->work;
            nextWork->state    = MAD_CHASER_LURK_STATE_IDLE;
            nextWork->subState = 0;
        }
    }
}
