/* Part of the factory lift library; see factory_lift.h. */

/// The handler that follows `factoryLiftLower` when bit 0 of
/// game flag 0x49 is clear.
s32 factoryLiftJamTurnBack(Task* task)
{
    FactoryLiftWork* work  = task->work;
    GfxCoord*        coord = task->extra.tmd->coords;
    s32              done  = 0;
    GfxMatrix*       mat;

    switch (work->yawStep) {
        case 0:
            work->yawVelocity = 0;
            work->yawStep++;
            break;
        case 1:
            if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
                Gp_EnqueueStageSnd6(SOUND_FACTORY_LIFT_TURN, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            } else {
                Gp_EnqueueStageSnd6(SOUND_NIGHT_FACTORY_LIFT_TURN, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
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
                    Gp_EnqueueStageSnd6(SOUND_FACTORY_LIFT_JAM, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                    padScriptSpawn(gFactoryDayJoltCmds, gFactoryDayJoltRecs);
                } else {
                    Gp_EnqueueStageSnd6(SOUND_NIGHT_FACTORY_LIFT_JAM, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
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
                    Gp_EnqueueStageSnd7(SOUND_FACTORY_LIFT_TURN, 1);
                    Gp_EnqueueStageSnd6(SOUND_FACTORY_LIFT_TURN_STOP, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                } else {
                    Gp_EnqueueStageSnd7(SOUND_NIGHT_FACTORY_LIFT_TURN, 1);
                    Gp_EnqueueStageSnd6(SOUND_NIGHT_FACTORY_LIFT_TURN_STOP, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
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
            Gp_EnqueueStageSnd7(SOUND_FACTORY_LIFT_TURN, 1);
            Gp_EnqueueStageSnd6(SOUND_FACTORY_LIFT_TURN_STOP, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
        } else {
            Gp_EnqueueStageSnd7(SOUND_NIGHT_FACTORY_LIFT_TURN, 1);
            Gp_EnqueueStageSnd6(SOUND_NIGHT_FACTORY_LIFT_TURN_STOP, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
        }
        work->yawStep = 4;
        done          = 1;
    }
    mat                       = (GfxMatrix*)&coord->coord;
    mat->rotationWords.m00M01 = ONE;
    mat->rotationWords.m02M10 = 0;
    mat->rotationWords.m11M12 = ONE;
    mat->rotationWords.m20M21 = 0;
    mat->rotationWords.m22    = ONE;
    RotMatrixY(work->yaw.halves.integer, &mat->mat);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return done;
}
