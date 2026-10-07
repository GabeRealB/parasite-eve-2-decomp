/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Moves the lurk alert laterally using the live rate in sixteenths of a frame.
///
/// distanceAtNormalRate is signed parent-coordinate units per normal-rate frame;
/// positive values move right along heading plus a quarter turn. The shifts
/// retain the signed low 20 product bits before division by 16 and s16 narrowing.
/// Requires the task's live work and model root.
static __inline__ void _madChaserLurkAlertMoveRight(Task* task, MadChaserWork* work, s32 distanceAtNormalRate)
{
    enum { MAD_CHASER_LURK_ALERT_RATE_FRACTION_BITS = 4 };
    MadChaserWork* rateWork;
    s16            sideHeading;
    s16            stepDistance;

    sideHeading                           = work->rotation.vy + ACTOR_TRANSFORM_ANGLE_TURN / 4;
    rateWork                              = task->work;
    stepDistance                          = (rateWork->animRate * distanceAtNormalRate) << (16 - MAD_CHASER_LURK_ALERT_RATE_FRACTION_BITS) >> 16;
    task->extra.tmd->coords->coord.t[0]  += ((rsin(sideHeading) << 4) * stepDistance) >> 16;
    task->extra.tmd->coords->coord.t[2]  += ((rcos(sideHeading) << 4) * stepDistance) >> 16;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Finishes the lurk alert's right step and enters combat walking.
///
/// Moves on previous counter values 29..41 inclusive by 30 parent-coordinate
/// units per normal-rate frame, scaled by the live animation rate. Post-increments
/// the u16 counter. A slot-1 boundary, jump or held pose clears busy, enters the
/// combat task state, selects walk behavior and resets its sub-state.
/// Requires initialized work and live root coordinates.
static void _madChaserLurkSidestepToCombat(Task* task)
{
    enum {
        MAD_CHASER_LURK_ALERT_STEP_START_FRAME = 29,
        MAD_CHASER_LURK_ALERT_STEP_FRAME_COUNT = 13,
        MAD_CHASER_LURK_ALERT_STEP_DISTANCE    = 30,
    };
    MadChaserWork* work;

    work = task->work;
    if ((u16)(work->stateFrames++ - MAD_CHASER_LURK_ALERT_STEP_START_FRAME) < MAD_CHASER_LURK_ALERT_STEP_FRAME_COUNT) {
        _madChaserLurkAlertMoveRight(task, work, MAD_CHASER_LURK_ALERT_STEP_DISTANCE);
    }
    if (_madChaserAnimHasBoundaryStatusInline(task)) {
        work->busy = 0;
        _madChaserEnterTaskState(task, MAD_CHASER_TASK_COMBAT);
        _madChaserSetBehaviorState(task, MAD_CHASER_COMBAT_STATE_WALK);
    }
}
