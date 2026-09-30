/* Part of the factory lift library; see factory_lift.h. */

/// The handler that follows `factoryLiftLower` when bit 0 of
/// game flag 0x49 is clear.
s32 factoryLiftJamTurnBack(Task* task)
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
            if (gGameSession->location.loc.stage == 2) {
                Gp_EnqueueStageSnd6(0x5217000F, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            } else {
                Gp_EnqueueStageSnd6(0x5317000F, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            }
            work->field_16++;
            break;
        case 2:
            work->field_8 += -0x18000;
            if (work->field_8 < -0x40000) {
                work->field_8 = -0x40000;
            }
            work->field_10.word += work->field_8;
            if (work->field_10.word < 0x3800000) {
                if (gGameSession->location.loc.stage == 2) {
                    Gp_EnqueueStageSnd6(0x52170012, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                    Gp_SpawnScript18(gFactoryDayJoltCmds, gFactoryDayJoltRecs);
                } else {
                    Gp_EnqueueStageSnd6(0x53170012, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                    Gp_SpawnScript18(gFactoryNightJoltCmds, gFactoryNightJoltRecs);
                }
                work->field_16++;
            }
            break;
        case 3:
            work->field_8 += 0xC000;
            if (work->field_8 > 0x20000) {
                work->field_8 = 0x20000;
            }
            work->field_10.word += work->field_8;
            if (work->field_10.word >= 0x4000000) {
                work->field_0 = GameFlag_GetNibble(0x49) | 1;
                GameFlag_SetNibble(0x49, work->field_0);
                work->field_10.word = 0x4000000;
                factoryLiftNotifyPanel(*(Task**)task->spawnArg2.pointer);
                if (gGameSession->location.loc.stage == 2) {
                    Gp_EnqueueStageSnd7(0x5217000F, 1);
                    Gp_EnqueueStageSnd6(0x52170011, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                } else {
                    Gp_EnqueueStageSnd7(0x5317000F, 1);
                    Gp_EnqueueStageSnd6(0x53170011, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                }
                work->field_16++;
            }
            break;
        default:
            done = 1;
            break;
    }

    if ((u8)(work->field_16 - 1) < 3 && Pad_CheckButtons(0, 1, 0x800) != 0 && (s16)work->field_14 >= 0xB) {
        work->field_0 = GameFlag_GetNibble(0x49) | 1;
        GameFlag_SetNibble(0x49, work->field_0);
        work->field_10.word = 0x4000000;
        factoryLiftNotifyPanel(*(Task**)task->spawnArg2.pointer);
        if (gGameSession->location.loc.stage == 2) {
            Gp_EnqueueStageSnd7(0x5217000F, 1);
            Gp_EnqueueStageSnd6(0x52170011, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
        } else {
            Gp_EnqueueStageSnd7(0x5317000F, 1);
            Gp_EnqueueStageSnd6(0x53170011, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
        }
        work->field_16 = 4;
        done           = 1;
    }
    mat                = (OverlayMat*)&coord->coord;
    mat->ident.m00_m01 = 0x1000;
    mat->ident.m02_m10 = 0;
    mat->ident.m11_m12 = 0x1000;
    mat->ident.m20_m21 = 0;
    mat->ident.m22     = 0x1000;
    RotMatrixY(work->field_10.halves.integer, &mat->mat);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return done;
}
