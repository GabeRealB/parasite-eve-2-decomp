/* Part of the factory lift library; see factory_lift.h. */

/// A turn requested while the lift is down: the yaw swings past 0x80, bangs
/// with a sound and a screen jolt script, then swings back to 0. It clears bit
/// 0 of nibble 0x49 so the turn is undone. Button skip and panel notification
/// as for the other moves.
s32 factoryLiftJamTurnOut(Task* task)
{
    FactoryLiftWork* work  = (FactoryLiftWork*)task->work;
    GfxCoord*        coord = task->extra.tmd->coords;
    s32              done  = 0;
    GfxMatrix*       mat;

    switch (work->field_16) {
        case 0:
            work->field_8 = 0;
            work->field_16++;
            break;
        case 1:
            if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
                Gp_EnqueueStageSnd6(SOUND_FACTORY_LIFT_TURN, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            } else {
                Gp_EnqueueStageSnd6(SOUND_NIGHT_FACTORY_LIFT_TURN, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            }
            work->field_16++;
            break;
        case 2:
            work->field_8 += 0x18000;
            if (work->field_8 > 0x40000) {
                work->field_8 = 0x40000;
            }
            work->field_10.word += work->field_8;
            if (work->field_10.word > 0x800000) {
                if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
                    Gp_EnqueueStageSnd6(SOUND_FACTORY_LIFT_JAM, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                    Gp_SpawnScript18(gFactoryDayJoltCmds, gFactoryDayJoltRecs);
                } else {
                    Gp_EnqueueStageSnd6(SOUND_NIGHT_FACTORY_LIFT_JAM, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                    Gp_SpawnScript18(gFactoryNightJoltCmds, gFactoryNightJoltRecs);
                }
                work->field_16++;
            }
            break;
        case 3:
            work->field_8 += -0xC000;
            if (work->field_8 < -0x20000) {
                work->field_8 = -0x20000;
            }
            work->field_10.word += work->field_8;
            if (work->field_10.word <= 0) {
                work->field_0 = GameFlag_GetNibble(GAME_FLAG_FACTORY_LIFT_POSITION) & 0xFE;
                GameFlag_SetNibble(GAME_FLAG_FACTORY_LIFT_POSITION, work->field_0);
                work->field_10.word = 0;
                factoryLiftNotifyPanel(*(Task**)task->spawnArg2.pointer);
                if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
                    Gp_EnqueueStageSnd7(SOUND_FACTORY_LIFT_TURN, 1);
                    Gp_EnqueueStageSnd6(SOUND_FACTORY_LIFT_TURN_STOP, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                } else {
                    Gp_EnqueueStageSnd7(SOUND_NIGHT_FACTORY_LIFT_TURN, 1);
                    Gp_EnqueueStageSnd6(SOUND_NIGHT_FACTORY_LIFT_TURN_STOP, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                }
                work->field_16++;
            }
            break;
        default:
            done = 1;
            break;
    }

    if ((u8)(work->field_16 - 1) < 3 && Pad_CheckButtons(0, 1, 0x800) != 0 && (s16)work->field_14 >= 0xB) {
        work->field_0 = GameFlag_GetNibble(GAME_FLAG_FACTORY_LIFT_POSITION) & 0xFE;
        GameFlag_SetNibble(GAME_FLAG_FACTORY_LIFT_POSITION, work->field_0);
        work->field_10.word = 0;
        factoryLiftNotifyPanel(*(Task**)task->spawnArg2.pointer);
        if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
            Gp_EnqueueStageSnd7(SOUND_FACTORY_LIFT_TURN, 1);
            Gp_EnqueueStageSnd6(SOUND_FACTORY_LIFT_TURN_STOP, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
        } else {
            Gp_EnqueueStageSnd7(SOUND_NIGHT_FACTORY_LIFT_TURN, 1);
            Gp_EnqueueStageSnd6(SOUND_NIGHT_FACTORY_LIFT_TURN_STOP, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
        }
        work->field_16 = 4;
        done           = 1;
    }
    mat                       = (GfxMatrix*)&coord->coord;
    mat->rotationWords.m00M01 = ONE;
    mat->rotationWords.m02M10 = 0;
    mat->rotationWords.m11M12 = ONE;
    mat->rotationWords.m20M21 = 0;
    mat->rotationWords.m22    = ONE;
    RotMatrixY(work->field_10.halves.integer, &mat->mat);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return done;
}
