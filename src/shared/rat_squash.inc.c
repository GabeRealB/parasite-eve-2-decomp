/* Part of the Rat library; see rat.h. */

/// Restores the saved death pose and applies its stored Q12 local Y scale.
///
/// Work/root and reserved scratch must be live and disjoint. The saved matrix
/// is the pose captured at death entry; `squashScale` is signed Q12 (4096 = 1).
/// Restoring it avoids compounding scale. Translation is copied from that pose;
/// unity X/Z factors preserve its other axes. Only the scratch matrix's 3x3
/// is initialized or read. The caller owns scale decay, invalidation and release.
static __inline__ void _ratApplyRootScale(GfxCoord* rootCoord, const RatWork* work, ActorScaleScratch* scratch)
{
    scratch->scale.vx = ONE;
    scratch->scale.vy = work->squashScale;
    scratch->scale.vz = ONE;
    rootCoord->coord  = work->savedRootMtx;
    gfxSetRotIdentity(&scratch->matrix);
    ScaleMatrix(&scratch->matrix, &scratch->scale);
    MulMatrix(&rootCoord->coord, &scratch->matrix);
}

/// Applies the corpse's decaying Q12 vertical scale to its saved root pose.
///
/// Requires live work/model and the matrix saved on death entry. Each call
/// subtracts 80 while scale exceeds 512, allowing the decrement to cross the
/// cutoff. Restoring the saved matrix avoids compounding scale; translation
/// and X/Z scale are retained. Marks the root dirty without composing it and
/// releases the reserved `ActorScaleScratch` before returning.
static void _ratSquash(Task* actor)
{
    enum {
        RAT_SQUASH_SCALE_CUTOFF = 512,
        RAT_SQUASH_SCALE_STEP   = 80,
    };

    GfxCoord*          rootCoord;
    ActorScaleScratch* scratchHead;
    ActorScaleScratch* scratch;
    RatWork*           work;

    scratchHead                             = SCRATCH_STACK_CURSOR(ActorScaleScratch);
    work                                    = actor->work;
    scratch                                 = scratchHead - 1;
    SCRATCH_STACK_CURSOR(ActorScaleScratch) = scratch;
    rootCoord                               = actor->extra.tmd->coords;
    if (work->squashScale > RAT_SQUASH_SCALE_CUTOFF) {
        work->squashScale -= RAT_SQUASH_SCALE_STEP;
    }
    _ratApplyRootScale(rootCoord, work, scratch);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleScratch);
}
