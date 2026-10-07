/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Moves the root right by 30 parent-coordinate units at normal animation rate.
///
/// Uses the supplied work's heading plus a quarter turn (4096 units per turn)
/// and reloads the task's live work for its rate in sixteenths of a frame.
/// Scaling retains the signed low 20 product bits before division by 16;
/// the displacement then narrows to s16. Requires live work and root coordinates.
static __inline__ void _madChaserAlertMoveRight(Task* task, MadChaserWork* work)
{
    enum { MAD_CHASER_ALERT_RIGHT_DISTANCE = 30 };
    s16 sideHeading;
    s16 stepDistance;

    stepDistance                          = _madChaserScaleByAnimRate(task, MAD_CHASER_ALERT_RIGHT_DISTANCE);
    sideHeading                           = work->rotation.vy + ACTOR_TRANSFORM_ANGLE_TURN / 4;
    task->extra.tmd->coords->coord.t[0]  += ((rsin(sideHeading) << 4) * stepDistance) >> 0x10;
    task->extra.tmd->coords->coord.t[2]  += ((rcos(sideHeading) << 4) * stepDistance) >> 0x10;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Steps right during the alert animation, then resumes combat walking.
///
/// Moves on previous counter values 29..41 inclusive, at 30 parent-coordinate
/// units per normal-rate frame. A slot-1 boundary, jump or settled pose clears
/// busy and enters the walk behavior at sub-state zero. Requires live work and
/// model coordinates; the frame counter is post-incremented as an unsigned halfword.
static void _madChaserAlertSidestep(Task* task)
{
    enum {
        MAD_CHASER_ALERT_STEP_START_FRAME = 29,
        MAD_CHASER_ALERT_STEP_FRAME_COUNT = 13,
    };
    MadChaserWork* work = task->work;

    if ((u16)(work->stateFrames++ - MAD_CHASER_ALERT_STEP_START_FRAME) < MAD_CHASER_ALERT_STEP_FRAME_COUNT) {
        _madChaserAlertMoveRight(task, work);
    }
    if (_madChaserAnimHasBoundaryStatus(task)) {
        MadChaserWork* nextWork;

        work->busy = 0;

        nextWork           = task->work;
        nextWork->state    = MAD_CHASER_COMBAT_STATE_WALK;
        nextWork->subState = 0;
    }
}
