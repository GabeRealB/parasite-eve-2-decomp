/* Part of the Maggot and Caterpillar library; see maggot_caterpillar.h. */

/// Progressively flattens the corpse root from a saved unscaled local matrix.
///
/// The vertical Q12 scale loses 80 each tick while above 512; the subtraction
/// can finish below 512. X and Z retain unit scale. `baseMatrix` must contain
/// the saved local transform so repeated calls do not compound the scale.
/// Releases its scale scratch and marks the root for recomposition.
static void _maggotCaterpillarSquash(Task* actor)
{
    enum {
        MAGGOT_CATERPILLAR_SQUASH_MIN_SCALE  = 512,
        MAGGOT_CATERPILLAR_SQUASH_SCALE_STEP = 80,
    };
    GfxCoord*              coord;
    ActorScaleScratch*     scratchEnd;
    ActorScaleScratch*     scratch;
    MaggotCaterpillarWork* work;

    scratchEnd                              = SCRATCH_STACK_CURSOR(ActorScaleScratch);
    work                                    = actor->work;
    scratch                                 = scratchEnd - 1;
    SCRATCH_STACK_CURSOR(ActorScaleScratch) = scratch;
    coord                                   = actor->extra.tmd->coords;
    if (work->vertical.squashScale > MAGGOT_CATERPILLAR_SQUASH_MIN_SCALE) {
        work->vertical.squashScale -= MAGGOT_CATERPILLAR_SQUASH_SCALE_STEP;
    }
    // Rebuild the root at one Q12 Y scale, then release the reserved block.
    // Arguments are stable pointer locals and are evaluated repeatedly.
#define MAGGOT_CATERPILLAR_REBUILD_CORPSE_SCALE(coord, work, scratch) \
    do {                                                              \
        (scratch)->scale.vx = ONE;                                    \
        (scratch)->scale.vy = (work)->vertical.squashScale;           \
        (scratch)->scale.vz = ONE;                                    \
        (coord)->coord      = (work)->baseMatrix;                     \
        gfxSetRotIdentity(&(scratch)->matrix);                        \
        ScaleMatrix(&(scratch)->matrix, &(scratch)->scale);           \
        MulMatrix(&(coord)->coord, &(scratch)->matrix);               \
        (coord)->composeStamp = GRAPHICS_COORD_DIRTY;                 \
        SCRATCH_STACK_RELEASE_BLOCK(ActorScaleScratch);               \
    } while (0)
    MAGGOT_CATERPILLAR_REBUILD_CORPSE_SCALE(coord, work, scratch);
#undef MAGGOT_CATERPILLAR_REBUILD_CORPSE_SCALE
}
