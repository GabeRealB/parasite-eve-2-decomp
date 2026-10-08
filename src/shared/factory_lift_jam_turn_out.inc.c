/* Part of the factory lift library; see factory_lift.h. */

/// Advances a blocked turn of the lowered lift, then returns it to home yaw.
///
/// Requires the live lift work, model and room-owned panel-task slot. The
/// outward swing passes 0x80 angle units, triggers the jam sound and jolt,
/// then reverses to zero. Completion or START after `FACTORY_LIFT_SKIP_FRAMES`
/// clears `FACTORY_LIFT_POSITION_TURNED` in both the saved flag and work copy.
/// Rebuilds local yaw each frame (4096 units per turn). Returns 1 when already
/// at rest or skipped, and 0 on the natural completion frame.
static s32 _factoryLiftJamTurnOut(Task* task)
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
            work->yawVelocity += 0x18000;
            if (work->yawVelocity > 0x40000) {
                work->yawVelocity = 0x40000;
            }
            work->yaw.word += work->yawVelocity;
            if (work->yaw.word > FACTORY_LIFT_JAM_YAW_OUT) {
                if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
                    sndEvtRequestStageScriptStart(SOUND_FACTORY_LIFT_JAM, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                    padScriptSpawn(gFactoryDayJoltCmds, gFactoryDayJoltRecs);
                } else {
                    sndEvtRequestStageScriptStart(SOUND_NIGHT_FACTORY_LIFT_JAM, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                    padScriptSpawn(gFactoryNightJoltCmds, gFactoryNightJoltRecs);
                }
                work->yawStep++;
            }
            break;
        case FACTORY_LIFT_STEP_SETTLE:
            work->yawVelocity += -0xC000;
            if (work->yawVelocity < -0x20000) {
                work->yawVelocity = -0x20000;
            }
            work->yaw.word += work->yawVelocity;
            if (work->yaw.word <= 0) {
                // Undo the blocked request in both copies so the next update stays at rest.
                work->position = gameFlagGetNibble(GAME_FLAG_FACTORY_LIFT_POSITION) & (u8)~FACTORY_LIFT_POSITION_TURNED;
                gameFlagSetNibble(GAME_FLAG_FACTORY_LIFT_POSITION, work->position);
                work->yaw.word = 0;
                _factoryLiftFinishTurn(task, coord);
                work->yawStep++;
            }
            break;
        default:
            movementComplete = 1;
            break;
    }

    // Skipping is allowed only during a moving phase and after the input delay.
    if ((u8)(work->yawStep - FACTORY_LIFT_STEP_START_SOUND) < FACTORY_LIFT_STEP_REST - FACTORY_LIFT_STEP_START_SOUND && padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_START) != 0 && work->moveFrames >= FACTORY_LIFT_SKIP_FRAMES) {
        // Undo the blocked request in both copies so the next update stays at rest.
        work->position = gameFlagGetNibble(GAME_FLAG_FACTORY_LIFT_POSITION) & (u8)~FACTORY_LIFT_POSITION_TURNED;
        gameFlagSetNibble(GAME_FLAG_FACTORY_LIFT_POSITION, work->position);
        work->yaw.word = 0;
        _factoryLiftFinishTurn(task, coord);
        work->yawStep    = FACTORY_LIFT_STEP_REST;
        movementComplete = 1;
    }
    _factoryLiftRebuildYaw(work, coord);
    return movementComplete;
}
