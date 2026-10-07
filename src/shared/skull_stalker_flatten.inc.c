/* Part of the Skull Stalker library; see skull_stalker.h. */

/// Rebuilds the second enemy's root coordinate from the matrix saved in
/// `savedRootMtx`, scaled along Y by `flattenScaleY`, which decays by 0x50 a frame
/// while above 0x200. The scaling matrix and its vector are staged in an
/// `ActorScaleScratch` block; the node's `composeStamp` is cleared so the next
/// `actorRenderComposeCoord` recomputes it.
void skullStalkerFlatten(Task* arg0)
{
    GfxCoord*          coord;
    ActorScaleScratch* head;
    ActorScaleScratch* scratch;
    SkullStalkerWork*  work;

    head                                    = SCRATCH_STACK_CURSOR(ActorScaleScratch);
    work                                    = arg0->work;
    scratch                                 = head - 1;
    SCRATCH_STACK_CURSOR(ActorScaleScratch) = scratch;
    coord                                   = arg0->extra.tmd->coords;
    if (work->flattenScaleY >= 0x201) {
        work->flattenScaleY -= 0x50;
    }
    scratch->scale.vx = ONE;
    scratch->scale.vy = work->flattenScaleY;
    scratch->scale.vz = ONE;
    coord->coord      = work->savedRootMtx;
    gfxSetRotIdentity(&scratch->matrix);
    ScaleMatrix(&scratch->matrix, &scratch->scale);
    MulMatrix(&coord->coord, &scratch->matrix);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleScratch);
}
