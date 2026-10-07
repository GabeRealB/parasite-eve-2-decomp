/* Part of the factory lift library; see factory_lift.h. */

/// Cutscene state 2: swings the model about X back up past 0, playing the
/// movement's sound as it gets there, and answers non-zero afterwards.
s32 factoryHatchClose(Task* task)
{
    FactoryHatchWork* work  = task->work;
    GfxCoord*         coord = task->extra.tmd->coords;
    GfxMatrix*        mat;
    s32               ret = 0;

    switch (work->step) {
        case 0:
            work->angularVelocity = 0;
            work->step++;
            break;
        case 1:
            work->angularVelocity += 0x20000;
            if (work->angularVelocity > 0x700000) {
                work->angularVelocity = 0x700000;
            }
            work->angle.word += work->angularVelocity;
            if (work->angle.word > 0) {
                if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
                    sndEvtRequestStageScriptStart(SOUND_FACTORY_HATCH_CLOSE, (s8)worldCoordGetOriginAudioPan(coord),
                                                  (s8)worldCoordGetOriginAudioDepth(coord));
                } else {
                    sndEvtRequestStageScriptStart(SOUND_NIGHT_FACTORY_HATCH_CLOSE, (s8)worldCoordGetOriginAudioPan(coord),
                                                  (s8)worldCoordGetOriginAudioDepth(coord));
                }
                work->step++;
            }
            break;
        default:
            ret = 1;
            break;
    }

    mat                       = (GfxMatrix*)&coord->coord;
    mat->rotationWords.m00M01 = ONE;
    mat->rotationWords.m02M10 = 0;
    mat->rotationWords.m11M12 = ONE;
    mat->rotationWords.m20M21 = 0;
    mat->rotationWords.m22    = ONE;
    RotMatrixX(work->angle.halves.integer, &mat->mat);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return ret;
}
