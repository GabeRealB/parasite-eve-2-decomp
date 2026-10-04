/* Part of the Maggot and Caterpillar library; see maggot_caterpillar.h. */

/// Squashes the model vertically: `field_3A0` shrinks by 0x50 a frame while
/// above 0x200, and the root coordinate becomes the matrix `field_370` scaled
/// on Y by `field_3A0` (0x1000 = 1), built through an `ActorScaleScratch`
/// block that is released again.
void maggotCaterpillarSquash(Task* arg0)
{
    GfxCoord*              coord;
    ActorScaleScratch*     head;
    ActorScaleScratch*     scratch;
    MaggotCaterpillarWork* work;

    head                                    = SCRATCH_STACK_CURSOR(ActorScaleScratch);
    work                                    = arg0->work;
    scratch                                 = head - 1;
    SCRATCH_STACK_CURSOR(ActorScaleScratch) = scratch;
    coord                                   = arg0->extra.tmd->coords;
    if (work->field_3A0 >= 0x201) {
        work->field_3A0 = (u16)work->field_3A0 - 0x50;
    }
    scratch->scale.vx                    = ONE;
    scratch->scale.vy                    = (s32)work->field_3A0;
    scratch->scale.vz                    = ONE;
    coord->coord                         = work->field_370;
    scratch->matrix.rotationWords.m00M01 = ONE;
    scratch->matrix.rotationWords.m02M10 = 0;
    scratch->matrix.rotationWords.m11M12 = ONE;
    scratch->matrix.rotationWords.m20M21 = 0;
    scratch->matrix.rotationWords.m22    = ONE;
    ScaleMatrix(&scratch->matrix.mat, &scratch->scale);
    MulMatrix(&coord->coord, &scratch->matrix.mat);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleScratch);
}
