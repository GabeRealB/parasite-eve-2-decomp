/* Part of the factory lift library; see factory_lift.h. */

/// Cutscene state 1: plays the movement's sound, swings the model about X
/// towards -0x300, overshooting and settling back on it, and answers non-zero
/// once it has settled.
s32 factoryHatchOpen(Task* task)
{
    FactoryHatchWork* work  = (FactoryHatchWork*)task->work;
    GfxCoord*         coord = task->extra.tmd->coords;
    OverlayMat*       mat;
    s32               ret = 0;

    switch (work->step) {
        case 0:
            work->field_0 = 0;
            if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
                Gp_EnqueueStageSnd6(0x5217000D, (s8)worldCoordGetOriginAudioPan(coord),
                                    (s8)worldCoordGetOriginAudioDepth(coord));
            } else {
                Gp_EnqueueStageSnd6(0x5317000D, (s8)worldCoordGetOriginAudioPan(coord),
                                    (s8)worldCoordGetOriginAudioDepth(coord));
            }
            work->step++;
            break;
        case 1:
            work->field_0 += -0x28000;
            if (work->field_0 < -0x300000) {
                work->field_0 = -0x300000;
            }
            work->field_4.word += work->field_0;
            if (work->field_4.word < -0x3000000) {
                work->step++;
            }
            break;
        case 2:
            work->field_0 += 0x40000;
            if (work->field_0 > 0x100000) {
                work->field_0 = 0x100000;
            }
            work->field_4.word += work->field_0;
            if (work->field_4.word >= -0x3000000) {
                work->field_4.word = -0x3000000;
                work->step++;
            }
            break;
        default:
            ret = 1;
            break;
    }

    mat               = (OverlayMat*)&coord->coord;
    mat->ident.m00M01 = ONE;
    mat->ident.m02M10 = 0;
    mat->ident.m11M12 = ONE;
    mat->ident.m20M21 = 0;
    mat->ident.m22    = ONE;
    RotMatrixX(work->field_4.halves.integer, &mat->mat);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return ret;
}
