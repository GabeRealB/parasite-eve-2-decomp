/* Part of the factory lift library; see factory_lift.h. */

/// Cutscene state 1: plays the movement's sound, swings the model about X
/// towards -0x300, overshooting and settling back on it, and answers non-zero
/// once it has settled.
s32 factoryHatchOpen(Task* task)
{
    FactoryHatchWork* work  = task->work;
    GfxCoord*         coord = task->extra.tmd->coords;
    GfxMatrix*        mat;
    s32               ret = 0;

    switch (work->step) {
        case 0:
            work->angularVelocity = 0;
            if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
                sndEvtRequestStageScriptStart(SOUND_FACTORY_HATCH_OPEN, (s8)worldCoordGetOriginAudioPan(coord),
                                              (s8)worldCoordGetOriginAudioDepth(coord));
            } else {
                sndEvtRequestStageScriptStart(SOUND_NIGHT_FACTORY_HATCH_OPEN, (s8)worldCoordGetOriginAudioPan(coord),
                                              (s8)worldCoordGetOriginAudioDepth(coord));
            }
            work->step++;
            break;
        case 1:
            work->angularVelocity += -0x28000;
            if (work->angularVelocity < -0x300000) {
                work->angularVelocity = -0x300000;
            }
            work->angle.word += work->angularVelocity;
            if (work->angle.word < -0x3000000) {
                work->step++;
            }
            break;
        case 2:
            work->angularVelocity += 0x40000;
            if (work->angularVelocity > 0x100000) {
                work->angularVelocity = 0x100000;
            }
            work->angle.word += work->angularVelocity;
            if (work->angle.word >= -0x3000000) {
                work->angle.word = -0x3000000;
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
