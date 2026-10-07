/* Part of the Maggot and Caterpillar library; see maggot_caterpillar.h. */

/// Squashes the model vertically: `vertical.squashScale` shrinks by 0x50 a frame while
/// above 0x200, and the root coordinate becomes the matrix `baseMatrix` scaled
/// on Y by `vertical.squashScale` (0x1000 = 1), built through an `ActorScaleScratch`
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
    if (work->vertical.squashScale >= 0x201) {
        work->vertical.squashScale -= 0x50;
    }
    scratch->scale.vx = ONE;
    scratch->scale.vy = work->vertical.squashScale;
    scratch->scale.vz = ONE;
    coord->coord      = work->baseMatrix;
    gfxSetRotIdentity(&scratch->matrix);
    ScaleMatrix(&scratch->matrix, &scratch->scale);
    MulMatrix(&coord->coord, &scratch->matrix);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleScratch);
}
