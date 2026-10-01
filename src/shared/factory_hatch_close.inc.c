/* Part of the factory lift library; see factory_lift.h. */

/// Cutscene state 2: swings the model about X back up past 0, playing the
/// movement's sound as it gets there, and answers non-zero afterwards.
s32 factoryHatchClose(Task* task)
{
    FactoryHatchWork* work  = (FactoryHatchWork*)task->work;
    GfxCoord*         coord = task->extra.tmd->coords;
    OverlayMat*       mat;
    s32               ret = 0;

    switch (work->step) {
        case 0:
            work->field_0 = 0;
            work->step++;
            break;
        case 1:
            work->field_0 += 0x20000;
            if (work->field_0 > 0x700000) {
                work->field_0 = 0x700000;
            }
            work->field_4.word += work->field_0;
            if (work->field_4.word > 0) {
                if (gGameSession->location.loc.stage == 2) {
                    Gp_EnqueueStageSnd6(0x5217000E, (s8)worldCoordGetOriginAudioPan(coord),
                                        (s8)worldCoordGetOriginAudioDepth(coord));
                } else {
                    Gp_EnqueueStageSnd6(0x5317000E, (s8)worldCoordGetOriginAudioPan(coord),
                                        (s8)worldCoordGetOriginAudioDepth(coord));
                }
                work->step++;
            }
            break;
        default:
            ret = 1;
            break;
    }

    mat                = (OverlayMat*)&coord->coord;
    mat->ident.m00_m01 = 0x1000;
    mat->ident.m02_m10 = 0;
    mat->ident.m11_m12 = 0x1000;
    mat->ident.m20_m21 = 0;
    mat->ident.m22     = 0x1000;
    RotMatrixX(work->field_4.halves.integer, &mat->mat);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return ret;
}
