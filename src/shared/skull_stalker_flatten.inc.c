/* Part of the Skull Stalker library; see skull_stalker.h. */

/// Applies the death's decaying Y scale to the saved root matrix.
///
/// Requires live work, a model root and a saved matrix from the current death
/// tick. Scale is Q12 (ONE is 1.0). The decrement occurs only above the cutoff;
/// it can step below that cutoff. Reserves one ActorScaleScratch and invalidates
/// the root's composition cache; the model is already hidden by the kill.
static void _skullStalkerFlatten(Task* task)
{
    enum { SKULL_STALKER_FLATTEN_CUTOFF_Q12 = 0x200,
           SKULL_STALKER_FLATTEN_STEP_Q12   = 0x50 };
    GfxCoord*          rootCoord;
    ActorScaleScratch* scratchHead;
    ActorScaleScratch* scratch;
    SkullStalkerWork*  work;

    scratchHead                             = SCRATCH_STACK_CURSOR(ActorScaleScratch);
    work                                    = task->work;
    scratch                                 = scratchHead - 1;
    SCRATCH_STACK_CURSOR(ActorScaleScratch) = scratch;
    rootCoord                               = task->extra.tmd->coords;
    if (work->flattenScaleY > SKULL_STALKER_FLATTEN_CUTOFF_Q12) {
        work->flattenScaleY -= SKULL_STALKER_FLATTEN_STEP_Q12;
    }
    /// Restores and rescales the root, then invalidates its composition cache.
    ///
    /// Captures rootCoord, scratch and work as stable live pointers. Borrows
    /// the caller's scratch reservation and leaves its release to the caller.
    /// Expands to a statement list; use only within a braced compound statement.
#define SKULL_STALKER_APPLY_ROOT_SCALE()            \
    scratch->scale.vx = ONE;                        \
    scratch->scale.vy = work->flattenScaleY;        \
    scratch->scale.vz = ONE;                        \
    rootCoord->coord  = work->savedRootMtx;         \
    gfxSetRotIdentity(&scratch->matrix);            \
    ScaleMatrix(&scratch->matrix, &scratch->scale); \
    MulMatrix(&rootCoord->coord, &scratch->matrix); \
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY

    SKULL_STALKER_APPLY_ROOT_SCALE();
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleScratch);
#undef SKULL_STALKER_APPLY_ROOT_SCALE
}
