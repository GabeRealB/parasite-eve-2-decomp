/* Part of the factory lift library; see factory_lift.h. */

/// The handler the model runs while bit 1 of game flag 0x49 is clear, and --
/// when bit 0 is set with it -- the handler that follows.
s32 factoryLiftLower(Task* task)
{
    FactoryLiftWork* work  = (FactoryLiftWork*)task->work;
    GfxCoord*        coord = task->extra.tmd->coords;
    s32              done  = 0;

    switch (work->field_17) {
        case 0:
            work->field_4 = 0;
            work->field_17++;
            break;
        case 1:
            if (gGameSession->location.loc.stage == 2) {
                Gp_EnqueueStageSnd6(0x52170008, (s8)worldCoordGetOriginAudioPan(coord), (s8)gpGetObjDepth(coord));
            } else {
                Gp_EnqueueStageSnd6(0x53170008, (s8)worldCoordGetOriginAudioPan(coord), (s8)gpGetObjDepth(coord));
            }
            work->field_17++;
            break;
        case 2:
            work->field_4 += 0xC000;
            if (work->field_4 > 0x30000) {
                work->field_4 = 0x30000;
            }
            work->field_C.word += work->field_4;
            if (work->field_C.word > 0) {
                work->field_17++;
            }
            break;
        case 3:
            work->field_4 += -0xC000;
            if (work->field_4 < -0xC000) {
                work->field_4 = -0xC000;
            }
            work->field_C.word += work->field_4;
            if (work->field_C.word <= 0) {
                factoryLiftNotifyPanel(*(Task**)task->spawnArg2.pointer);
                if (gGameSession->location.loc.stage == 2) {
                    Gp_EnqueueStageSnd7(0x52170008, 1);
                    Gp_EnqueueStageSnd6(0x52170010, (s8)worldCoordGetOriginAudioPan(coord), (s8)gpGetObjDepth(coord));
                } else {
                    Gp_EnqueueStageSnd7(0x53170008, 1);
                    Gp_EnqueueStageSnd6(0x53170010, (s8)worldCoordGetOriginAudioPan(coord), (s8)gpGetObjDepth(coord));
                }
                work->field_17++;
            }
            break;
        default:
            done = 1;
            break;
    }

    if ((u8)(work->field_17 - 1) < 3 && Pad_CheckButtons(0, 1, 0x800) != 0 && (s16)work->field_14 >= 0xB) {
        factoryLiftNotifyPanel(*(Task**)task->spawnArg2.pointer);
        if (gGameSession->location.loc.stage == 2) {
            Gp_EnqueueStageSnd7(0x52170008, 1);
            Gp_EnqueueStageSnd6(0x52170010, (s8)worldCoordGetOriginAudioPan(coord), (s8)gpGetObjDepth(coord));
        } else {
            Gp_EnqueueStageSnd7(0x53170008, 1);
            Gp_EnqueueStageSnd6(0x53170010, (s8)worldCoordGetOriginAudioPan(coord), (s8)gpGetObjDepth(coord));
        }
        done               = 1;
        work->field_C.word = 0;
        work->field_17     = 4;
    }
    coord->coord.t[1]   = work->field_C.halves.integer;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return done;
}
