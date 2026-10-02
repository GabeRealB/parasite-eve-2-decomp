/* Part of the factory lift library; see factory_lift.h. */

/// Raised-lift turn from 0 to a quarter turn: accelerates the yaw, overshoots,
/// settles on 0x400 with start/stop sounds and notifies the panel. A button
/// press after 11 frames snaps it to the end. Answers non-zero when finished.
s32 factoryLiftTurnOut(Task* task)
{
    FactoryLiftWork* work  = (FactoryLiftWork*)task->work;
    GfxCoord*        coord = task->extra.tmd->coords;
    s32              done  = 0;
    OverlayMat*      mat;

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
            if (work->field_10.word > 0x4000000) {
                work->field_16++;
            }
            break;
        case 3:
            work->field_8 += -0x8000;
            if (work->field_8 < -0x20000) {
                work->field_8 = -0x20000;
            }
            work->field_10.word += work->field_8;
            if (work->field_10.word <= 0x4000000) {
                factoryLiftNotifyPanel(*(Task**)task->spawnArg2.pointer);
                if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
                    Gp_EnqueueStageSnd7(SOUND_FACTORY_LIFT_TURN, 1);
                    Gp_EnqueueStageSnd6(SOUND_FACTORY_LIFT_TURN_STOP, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                } else {
                    Gp_EnqueueStageSnd7(SOUND_NIGHT_FACTORY_LIFT_TURN, 1);
                    Gp_EnqueueStageSnd6(SOUND_NIGHT_FACTORY_LIFT_TURN_STOP, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                }
                work->field_10.word = 0x4000000;
                work->field_16++;
            }
            break;
        default:
            done = 1;
            break;
    }

    if ((u8)(work->field_16 - 1) < 3 && Pad_CheckButtons(0, 1, 0x800) != 0 && (s16)work->field_14 >= 0xB) {
        factoryLiftNotifyPanel(*(Task**)task->spawnArg2.pointer);
        if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
            Gp_EnqueueStageSnd7(SOUND_FACTORY_LIFT_TURN, 1);
            Gp_EnqueueStageSnd6(SOUND_FACTORY_LIFT_TURN_STOP, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
        } else {
            Gp_EnqueueStageSnd7(SOUND_NIGHT_FACTORY_LIFT_TURN, 1);
            Gp_EnqueueStageSnd6(SOUND_NIGHT_FACTORY_LIFT_TURN_STOP, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
        }
        done                = 1;
        work->field_10.word = 0x4000000;
        work->field_16      = 4;
    }
    mat               = (OverlayMat*)&coord->coord;
    mat->ident.m00M01 = ONE;
    mat->ident.m02M10 = 0;
    mat->ident.m11M12 = ONE;
    mat->ident.m20M21 = 0;
    mat->ident.m22    = ONE;
    RotMatrixY(work->field_10.halves.integer, &mat->mat);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return done;
}
