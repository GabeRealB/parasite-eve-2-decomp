/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Restores an unscaled root and multiplies a Q12 Y scale into its rotation.
///
/// The caller supplies live root, work and scratch storage. Translation is
/// restored from `savedRootMtx`; the caller then invalidates composition.
static __inline__ void _sucklercephRescaleRoot(GfxCoord* rootCoord, SucklercephWork* work, ActorScaleScratch* scratch)
{
    scratch->scale.vx = ONE;
    scratch->scale.vy = work->flattenScaleY;
    scratch->scale.vz = ONE;
    rootCoord->coord  = work->savedRootMtx;
    gfxSetRotIdentity(&scratch->matrix);
    ScaleMatrix(&scratch->matrix, &scratch->scale);
    MulMatrix(&rootCoord->coord, &scratch->matrix);
}

/// Flattens the dying Sucklerceph's root along its local Y axis.
///
/// Requires live root/work storage and `savedRootMtx` captured when the death
/// countdown ended. The Q12 Y scale falls by 80 while above 512, so it may
/// finish below that threshold; X/Z stay at `ONE`. Rebuilds from the saved
/// matrix each call to avoid compounding scale and invalidates composition.
/// Releases its `ActorScaleScratch` block before return.
static void _sucklercephFlatten(Task* task)
{
    enum { SUCKLERCEPH_FLATTEN_SCALE_THRESHOLD = 512,
           SUCKLERCEPH_FLATTEN_SCALE_STEP      = 80 };

    GfxCoord*          rootCoord;
    ActorScaleScratch* scratch;
    SucklercephWork*   work;

    scratch   = SCRATCH_STACK_RESERVE_BLOCK(ActorScaleScratch);
    work      = task->work;
    rootCoord = task->extra.tmd->coords;
    if (work->flattenScaleY >= SUCKLERCEPH_FLATTEN_SCALE_THRESHOLD + 1) {
        work->flattenScaleY -= SUCKLERCEPH_FLATTEN_SCALE_STEP;
    }
    _sucklercephRescaleRoot(rootCoord, work, scratch);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleScratch);
}
