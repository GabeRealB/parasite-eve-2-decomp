/* Part of the Rat library; see rat.h. */

/// Reduces the death Y scale by 0x50 a frame down to 0x200 and rebuilds the
/// root rotation as the saved transform times that Y scale, marking the
/// coordinate dirty.
void ratSquash(Task* arg0)
{
    GfxCoord*          coord;
    ActorScaleScratch* head;
    ActorScaleScratch* scratch;
    RatWork*           work;

    head                                    = SCRATCH_STACK_CURSOR(ActorScaleScratch);
    work                                    = arg0->work;
    scratch                                 = head - 1;
    SCRATCH_STACK_CURSOR(ActorScaleScratch) = scratch;
    coord                                   = arg0->extra.tmd->coords;
    if (work->squashScale >= 0x201) {
        work->squashScale -= 0x50;
    }
    scratch->scale.vx = ONE;
    scratch->scale.vy = work->squashScale;
    scratch->scale.vz = ONE;
    coord->coord      = work->savedRootMtx;
    gfxSetRotIdentity(&scratch->matrix.mat);
    ScaleMatrix(&scratch->matrix.mat, &scratch->scale);
    MulMatrix(&coord->coord, &scratch->matrix.mat);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleScratch);
}
