/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Rebuilds the first enemy's root coordinate from the transform saved in
/// `savedRootMtx`, scaled along Y by `flattenScaleY`, which decays by 0x50 a frame
/// while it stays above 0x200. The scale matrix and its `VECTOR` live in an
/// `ActorScaleScratch` block; the node's `composeStamp` is cleared so the next
/// `actorRenderComposeCoord` recomputes it.
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
    if (work->flattenScaleY >= 0x201) {
        work->flattenScaleY -= 0x50;
    }
    scratch->scale.vx                    = ONE;
    scratch->scale.vy                    = work->flattenScaleY;
    scratch->scale.vz                    = ONE;
    coord->coord                         = work->savedRootMtx;
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
