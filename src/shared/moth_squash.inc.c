/* Part of the Moth library; see moth.h. */

/// Reduces the death Y scale by 0x50 a frame down to 0x200, rebuilds the root
/// rotation as the saved transform times that Y scale and recomposes the
/// coordinate.
void mothSquash(Task* arg0)
{
    GfxCoord*          coord;
    ActorScaleScratch* head;
    ActorScaleScratch* scratch;
    MothWork*          work;

    head                                    = SCRATCH_STACK_CURSOR(ActorScaleScratch);
    work                                    = arg0->work;
    scratch                                 = head - 1;
    SCRATCH_STACK_CURSOR(ActorScaleScratch) = scratch;
    coord                                   = arg0->extra.tmd->coords;
    if (work->squashScale >= 0x201) {
        work->squashScale -= 0x50;
    }
    scratch->scale.vx                    = ONE;
    scratch->scale.vy                    = work->squashScale;
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
    actorRenderComposeCoord(coord);
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleScratch);
}
