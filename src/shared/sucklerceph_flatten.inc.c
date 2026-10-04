/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Rebuilds the first enemy's root coordinate from the transform saved in
/// `field_28C`, scaled along Y by `field_2CA`, which decays by 0x50 a frame
/// while it stays above 0x200. The scale matrix and its `VECTOR` live in an
/// `ActorScaleScratch` block; the node's `composeStamp` is cleared so the next
/// `Gp_UpdateCoord` recomputes it.
void sucklercephFlatten(Task* arg0)
{
    GfxCoord*          coord;
    ActorScaleScratch* head;
    ActorScaleScratch* scratch;
    SucklercephWork*   work;

    head                                    = SCRATCH_STACK_CURSOR(ActorScaleScratch);
    work                                    = arg0->work;
    scratch                                 = head - 1;
    SCRATCH_STACK_CURSOR(ActorScaleScratch) = scratch;
    coord                                   = arg0->extra.tmd->coords;
    if (work->field_2CA >= 0x201) {
        work->field_2CA = (u16)work->field_2CA - 0x50;
    }
    scratch->scale.vx                    = ONE;
    scratch->scale.vy                    = (s32)work->field_2CA;
    scratch->scale.vz                    = ONE;
    coord->coord                         = work->field_28C;
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
