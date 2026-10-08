/* Part of the Moth library; see moth.h. */

/// Restores the saved death pose and scales its root rotation vertically.
///
/// Borrows live work/root and a disjoint reserved scale scratch block; the
/// caller owns invalidation, composition and release. The signed Q12 Y factor
/// uses 4096 as unity; X/Z stay at unity and saved translation is preserved.
static __inline__ void _mothApplyRootScale(GfxCoord* rootCoord, const MothWork* work, ActorScaleScratch* scratch)
{
    scratch->scale.vx = ONE;
    scratch->scale.vy = work->squashScale;
    scratch->scale.vz = ONE;
    rootCoord->coord  = work->savedRootMtx;
    gfxSetRotIdentity(&scratch->matrix);
    ScaleMatrix(&scratch->matrix, &scratch->scale);
    MulMatrix(&rootCoord->coord, &scratch->matrix);
}

/// Applies the death's decaying Q12 vertical scale to its saved root pose.
///
/// Requires live work, model root and the pose saved when death began. Each
/// call subtracts 80 while scale exceeds 512 (the decrement can cross below
/// the cutoff), restores the saved matrix to avoid compounding scale, then
/// invalidates and composes the root. X/Z scale remain ONE; translation comes
/// from the saved pose. Releases its ActorScaleScratch block before return.
static void _mothSquash(Task* task)
{
    enum {
        MOTH_SQUASH_SCALE_CUTOFF = 0x200,
        MOTH_SQUASH_SCALE_STEP   = 0x50
    };

    GfxCoord*          rootCoord;
    ActorScaleScratch* scratchHead;
    ActorScaleScratch* scratch;
    MothWork*          work;

    scratchHead                             = SCRATCH_STACK_CURSOR(ActorScaleScratch);
    work                                    = task->work;
    scratch                                 = scratchHead - 1;
    SCRATCH_STACK_CURSOR(ActorScaleScratch) = scratch;
    rootCoord                               = task->extra.tmd->coords;
    if (work->squashScale > MOTH_SQUASH_SCALE_CUTOFF) {
        work->squashScale -= MOTH_SQUASH_SCALE_STEP;
    }
    _mothApplyRootScale(rootCoord, work, scratch);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(rootCoord);
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleScratch);
}
