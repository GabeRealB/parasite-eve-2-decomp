/* Part of the Maggot and Caterpillar library; see maggot_caterpillar.h. */

/// Squashes the model vertically: `field_3A0` shrinks by 0x50 a frame while
/// above 0x200, and the root coordinate becomes the matrix `field_370` scaled
/// on Y by `field_3A0` (0x1000 = 1), built through a 0x30-byte scratchpad
/// block that is released again.
void maggotCaterpillarSquash(Task* arg0)
{
    GfxCoord*          coord;
    ActorScaleScratch* head;
    ActorScaleScratch* scratch;
    Actor105500Work*   work;

    head                                    = SCRATCH_STACK_CURSOR(ActorScaleScratch);
    work                                    = arg0->work;
    scratch                                 = head - 1;
    SCRATCH_STACK_CURSOR(ActorScaleScratch) = scratch;
    coord                                   = arg0->extra.tmd->coords;
    if (work->field_3A0 >= 0x201) {
        work->field_3A0 = (u16)work->field_3A0 - 0x50;
    }
    scratch->scale.vx          = 0x1000;
    scratch->scale.vy          = (s32)work->field_3A0;
    scratch->scale.vz          = 0x1000;
    coord->coord               = work->field_370;
    scratch->mat.ident.m00_m01 = 0x1000;
    scratch->mat.ident.m02_m10 = 0;
    scratch->mat.ident.m11_m12 = 0x1000;
    scratch->mat.ident.m20_m21 = 0;
    scratch->mat.ident.m22     = 0x1000;
    ScaleMatrix(&scratch->mat.mat, &scratch->scale);
    MulMatrix(&coord->coord, &scratch->mat.mat);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleScratch);
}
