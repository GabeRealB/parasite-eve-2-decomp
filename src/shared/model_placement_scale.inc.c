/* Part of the model placement library; see model_placement.h. */

/// Rebuilds a root matrix from a saved matrix and prepared per-axis scale factors.
///
/// `scratch` is a live `ActorScaleScratch` with its scale vector initialized.
/// Its matrix translation is unused. Copies the saved translation and retains
/// the root's parent; composition must refresh its invalidated cache before use.
static inline void _modelPlacementApplyRootScale(GfxCoord* rootCoord, const MATRIX* unscaledMatrix, ActorScaleScratch* scratch)
{
    rootCoord->coord = *unscaledMatrix;

    gfxSetRotIdentity(&scratch->matrix.mat);

    ScaleMatrix(&scratch->matrix.mat, &scratch->scale);
    MulMatrix(&rootCoord->coord, &scratch->matrix.mat);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Replaces a model's root transform with a saved transform scaled in local axes.
///
/// `modelTask` must have a live `TASK_BODY_TMD` model with coordinate 0, and
/// `unscaledMatrix` must be a readable, word-aligned full `MATRIX`.
/// `scale` is a signed factor with 12 fractional bits (`ONE` = 1.0).
/// `MODEL_PLACEMENT_SCALE_Y_ONLY` scales local Y; any nonzero `uniformScale`,
/// conventionally `MODEL_PLACEMENT_SCALE_UNIFORM`, scales all three axes.
/// Translation comes from the saved matrix and is not scaled; the root's
/// parent stays intact. Repeated calls with an unchanged saved matrix do not
/// compound the scale.
///
/// Marks composition stale without rebuilding the cached matrix. Borrows and
/// releases one `ActorScaleScratch` block, requiring that much free space on
/// the initialized scratch stack. No pointers are retained; GTE rotation and
/// result registers are changed by the matrix multiply.
static void _modelPlacementSetScaled(Task* modelTask, const MATRIX* unscaledMatrix, s16 scale, s32 uniformScale)
{
    ActorScaleScratch* stackTop;
    ActorScaleScratch* scratch;
    GfxCoord*          rootCoord;

    // Keep the cursor update and the retained block address as separate evaluations.
    stackTop                                = SCRATCH_STACK_CURSOR(ActorScaleScratch);
    SCRATCH_STACK_CURSOR(ActorScaleScratch) = stackTop - 1;
    scratch                                 = stackTop - 1;
    rootCoord                               = modelTask->extra.tmd->coords;

    if (uniformScale == MODEL_PLACEMENT_SCALE_Y_ONLY) {
        scratch->scale.vx = ONE;
        scratch->scale.vy = scale;
        scratch->scale.vz = ONE;
    } else {
        scratch->scale.vx = scale;
        scratch->scale.vy = scale;
        scratch->scale.vz = scale;
    }

    // Multiply a local-axis scale into the saved rotation without scaling translation.
    _modelPlacementApplyRootScale(rootCoord, unscaledMatrix, scratch);
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleScratch);
}
