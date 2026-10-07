/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Moves the root laterally in its parent frame, scaled by animation rate.
///
/// Requires the task's live work and root. Scaling retains the signed low 20
/// product bits before division by 16; the displacement then narrows to s16.
static __inline__ void _madChaserAlertMoveRight(Task* task, MadChaserWork* work)
{
    s16 sideHeading;
    s16 stepDistance;

    stepDistance                          = _madChaserScaleByAnimRate(task, 0x1E);
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
