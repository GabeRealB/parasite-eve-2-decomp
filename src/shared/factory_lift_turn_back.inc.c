/* Part of the factory lift library; see factory_lift.h. */

/// Advances the raised lift's return quarter-turn and settles at home yaw zero.
///
/// Requires the live lift work, model and room-owned panel-task slot. Uses
/// 16.16 angle and velocity, with 4096 angle units per turn; overshoots zero,
/// reverses and clamps on return. START after `FACTORY_LIFT_SKIP_FRAMES`
/// snaps to zero. Rebuilds local yaw each frame. Returns 1 when already at
/// rest or skipped, and 0 on the natural completion frame.
static s32 _factoryLiftTurnBack(Task* task)
{
    FactoryLiftWork* work             = task->work;
    GfxCoord*        coord            = task->extra.tmd->coords;
    s32              movementComplete = 0;

    switch (work->yawStep) {
        case FACTORY_LIFT_STEP_RESET:
            work->yawVelocity = 0;
            work->yawStep++;
            break;
        case FACTORY_LIFT_STEP_START_SOUND:
            if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
                sndEvtRequestStageScriptStart(SOUND_FACTORY_LIFT_TURN, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            } else {
                sndEvtRequestStageScriptStart(SOUND_NIGHT_FACTORY_LIFT_TURN, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            }
            work->yawStep++;
            break;
        case FACTORY_LIFT_STEP_OUTBOUND:
            work->yawVelocity += -0x18000;
            if (work->yawVelocity < -0x40000) {
                work->yawVelocity = -0x40000;
            }
            work->yaw.word += work->yawVelocity;
            if (work->yaw.word < 0) {
                work->yawStep++;
            }
            break;
        case FACTORY_LIFT_STEP_SETTLE:
            work->yawVelocity += 0x8000;
            if (work->yawVelocity > 0x20000) {
                work->yawVelocity = 0x20000;
            }
            work->yaw.word += work->yawVelocity;
            if (work->yaw.word >= 0) {
                _factoryLiftFinishTurn(task, coord);
                work->yaw.word = 0;
                work->yawStep++;
            }
            break;
        default:
            movementComplete = 1;
            break;
    }

    // Skipping is allowed only during a moving phase and after the input delay.
    if ((u8)(work->yawStep - FACTORY_LIFT_STEP_START_SOUND) < FACTORY_LIFT_STEP_REST - FACTORY_LIFT_STEP_START_SOUND && padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_START) != 0 && work->moveFrames >= FACTORY_LIFT_SKIP_FRAMES) {
        _factoryLiftFinishTurn(task, coord);
        movementComplete = 1;
        work->yaw.word   = 0;
        work->yawStep    = FACTORY_LIFT_STEP_REST;
    }
    _factoryLiftRebuildYaw(work, coord);
    return movementComplete;
}
