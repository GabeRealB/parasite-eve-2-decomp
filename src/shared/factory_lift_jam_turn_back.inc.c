/* Part of the factory lift library; see factory_lift.h. */

/// The handler that follows `factoryLiftLower` when bit 0 of
/// game flag 0x49 is clear.
s32 factoryLiftJamTurnBack(Task* task)
{
    FactoryLiftWork* work  = task->work;
    GfxCoord*        coord = task->extra.tmd->coords;
    s32              done  = 0;

    switch (work->yawStep) {
        case 0:
            work->yawVelocity = 0;
            work->yawStep++;
            break;
        case 1:
            if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
                sndEvtRequestStageScriptStart(SOUND_FACTORY_LIFT_TURN, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            } else {
                sndEvtRequestStageScriptStart(SOUND_NIGHT_FACTORY_LIFT_TURN, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            }
            work->yawStep++;
            break;
        case 2:
            work->yawVelocity += -0x18000;
            if (work->yawVelocity < -0x40000) {
                work->yawVelocity = -0x40000;
            }
            work->yaw.word += work->yawVelocity;
            if (work->yaw.word < 0x3800000) {
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
        case 3:
            work->yawVelocity += 0xC000;
            if (work->yawVelocity > 0x20000) {
                work->yawVelocity = 0x20000;
            }
            work->yaw.word += work->yawVelocity;
            if (work->yaw.word >= FACTORY_LIFT_YAW_TURNED) {
                work->position = gameFlagGetNibble(GAME_FLAG_FACTORY_LIFT_POSITION) | FACTORY_LIFT_POSITION_TURNED;
                gameFlagSetNibble(GAME_FLAG_FACTORY_LIFT_POSITION, work->position);
                work->yaw.word = FACTORY_LIFT_YAW_TURNED;
                factoryLiftNotifyPanel(*(Task**)task->spawnArg2.pointer);
                if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
                    sndEvtRequestStageScriptStop(SOUND_FACTORY_LIFT_TURN, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                    sndEvtRequestStageScriptStart(SOUND_FACTORY_LIFT_TURN_STOP, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                } else {
                    sndEvtRequestStageScriptStop(SOUND_NIGHT_FACTORY_LIFT_TURN, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                    sndEvtRequestStageScriptStart(SOUND_NIGHT_FACTORY_LIFT_TURN_STOP, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                }
                work->yawStep++;
            }
            break;
        default:
            done = 1;
            break;
    }

    if ((u8)(work->yawStep - 1) < 3 && padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_START) != 0 && work->moveFrames >= FACTORY_LIFT_SKIP_FRAMES) {
        work->position = gameFlagGetNibble(GAME_FLAG_FACTORY_LIFT_POSITION) | FACTORY_LIFT_POSITION_TURNED;
        gameFlagSetNibble(GAME_FLAG_FACTORY_LIFT_POSITION, work->position);
        work->yaw.word = FACTORY_LIFT_YAW_TURNED;
        factoryLiftNotifyPanel(*(Task**)task->spawnArg2.pointer);
        if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
            sndEvtRequestStageScriptStop(SOUND_FACTORY_LIFT_TURN, SOUND_SCRIPT_STOP_KEEP_RELEASE);
            sndEvtRequestStageScriptStart(SOUND_FACTORY_LIFT_TURN_STOP, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
        } else {
            sndEvtRequestStageScriptStop(SOUND_NIGHT_FACTORY_LIFT_TURN, SOUND_SCRIPT_STOP_KEEP_RELEASE);
            sndEvtRequestStageScriptStart(SOUND_NIGHT_FACTORY_LIFT_TURN_STOP, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
        }
        work->yawStep = 4;
        done          = 1;
    }
    gfxSetRotIdentity(&coord->coord);
    RotMatrixY(work->yaw.halves.integer, &coord->coord);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return done;
}
