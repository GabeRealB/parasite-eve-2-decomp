/* Part of the Zebra and Ivory Stalker library; see stalker_zebra_ivory.h. */

/// Samples a model part's world X/Z into signed halfwords, leaving output Y untouched.
///
/// `partIndex` must index the live model's coordinates, whose root is beneath
/// `gGfxViewCoord`. The writable output supplies X/Z storage and is not retained.
/// Refreshes view and part caches, removes the view transform, then marks the
/// sampled part dirty again. Requires composed ancestors and the initialized
/// scratch stack used by `gfxMakeRelativeTransform`.
static void _stalkerZebraIvoryReadPartWorldXZ(Task* task, s16 partIndex, SVECTOR3* worldPosition)
{
    MATRIX    partToWorld;
    GfxCoord* partCoord;
    GfxCoord* parts;

    parts                      = task->extra.tmd->coords;
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    partCoord                  = &parts[partIndex];
    actorRenderComposeCoord(&gGfxViewCoord);
    partCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(partCoord);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &partCoord->workm, &partToWorld);
    worldPosition->vx       = partToWorld.t[0];
    worldPosition->vz       = partToWorld.t[2];
    partCoord->composeStamp = GRAPHICS_COORD_DIRTY;
}
