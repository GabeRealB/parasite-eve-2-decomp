/* Part of the Maggot and Caterpillar library; see maggot_caterpillar.h. */

/// Shrinks the corpse model third coordinate node uniformly to one sixteenth.
///
/// The task must own a model with at least three coordinates. The scale
/// multiplies its existing local rotation and leaves translation unchanged.
/// One scale scratch block is reserved and released. The caller controls
/// recomposition; this routine leaves the compose stamp intact.
static void _maggotCaterpillarShrinkNode2(Task* actor)
{
    void**             cursorSlot;
    ActorScaleScratch* scratchEnd;
    ActorScaleScratch* scratch;
    GfxCoord*          coords;

    cursorSlot                                     = SCRATCH_HEAD_ADDR;
    scratchEnd                                     = SCRATCH_HEAD_AT(cursorSlot, ActorScaleScratch);
    scratch                                        = scratchEnd - 1;
    SCRATCH_HEAD_AT(cursorSlot, ActorScaleScratch) = scratch;
    coords                                         = actor->extra.tmd->coords;

    scratch->scale.vx = ONE / 16;
    scratch->scale.vy = ONE / 16;
    scratch->scale.vz = ONE / 16;
    gfxSetRotIdentity(&scratch->matrix);
    ScaleMatrix(&scratch->matrix, &scratch->scale);
    MulMatrix(&coords[2].coord, &scratch->matrix);
    SCRATCH_POP_AT(cursorSlot, ActorScaleScratch);
}
