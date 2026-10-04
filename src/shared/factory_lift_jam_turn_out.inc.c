/* Part of the factory lift library; see factory_lift.h. */

/// A turn requested while the lift is down: the yaw swings past 0x80, bangs
/// with a sound and a screen jolt script, then swings back to 0. It clears bit
/// 0 of nibble 0x49 so the turn is undone. Button skip and panel notification
/// as for the other moves.
s32 factoryLiftJamTurnOut(Task* task)
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
            work->yawVelocity += 0x18000;
            if (work->yawVelocity > 0x40000) {
                work->yawVelocity = 0x40000;
            }
            work->yaw.word += work->yawVelocity;
            if (work->yaw.word > 0x800000) {
                if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
                    Gp_EnqueueStageSnd6(SOUND_FACTORY_LIFT_JAM, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                    Gp_SpawnScript18(gFactoryDayJoltCmds, gFactoryDayJoltRecs);
                } else {
                    Gp_EnqueueStageSnd6(SOUND_NIGHT_FACTORY_LIFT_JAM, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                    Gp_SpawnScript18(gFactoryNightJoltCmds, gFactoryNightJoltRecs);
                }
                work->yawStep++;
            }
            break;
        case 3:
            work->yawVelocity += -0xC000;
            if (work->yawVelocity < -0x20000) {
                work->yawVelocity = -0x20000;
            }
            work->yaw.word += work->yawVelocity;
            if (work->yaw.word <= 0) {
                work->position = gameFlagGetNibble(GAME_FLAG_FACTORY_LIFT_POSITION) & (u8)~FACTORY_LIFT_POSITION_TURNED;
                gameFlagSetNibble(GAME_FLAG_FACTORY_LIFT_POSITION, work->position);
                work->yaw.word = 0;
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

    if ((u8)(work->yawStep - 1) < 3 && Pad_CheckButtons(0, 1, 0x800) != 0 && work->moveFrames >= FACTORY_LIFT_SKIP_FRAMES) {
        work->position = gameFlagGetNibble(GAME_FLAG_FACTORY_LIFT_POSITION) & (u8)~FACTORY_LIFT_POSITION_TURNED;
        gameFlagSetNibble(GAME_FLAG_FACTORY_LIFT_POSITION, work->position);
        work->yaw.word = 0;
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
