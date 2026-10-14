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

/// Moves the model root in world X/Z so a selected part stays at its anchor.
///
/// The root must be parented directly to `gGfxViewCoord`, and `partIndex` must
/// index its live coordinate array. Reads the anchor's signed-halfword X/Z;
/// leaves root Y and rotation unchanged and retains no anchor pointer.
/// Requires composed ancestors and an initialized scratch stack with the
/// capacity used by `gfxMakeRelativeTransform`. Zebra recomposes the root
/// after moving it; Ivory marks it dirty before measuring the original offset.
static void _stalkerZebraIvoryPinPartXZ(Task* task, s16 partIndex, const SVECTOR3* anchorPosition)
{
    MATRIX    rootToWorld;
    MATRIX    partToWorld;
    GfxCoord* partCoord;
    GfxCoord* parts;

    parts                      = task->extra.tmd->coords;
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    partCoord                  = &parts[partIndex];
    actorRenderComposeCoord(&gGfxViewCoord);
#if !STALKER_ZEBRA_IVORY_PIN_RECOMPOSES_ROOT
    parts[0].composeStamp = GRAPHICS_COORD_DIRTY;
#endif
    partCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(partCoord);
    // Remove the view transform before comparing the root and anchored part.
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &parts[0].workm, &rootToWorld);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &partCoord->workm, &partToWorld);
    parts[0].coord.t[0]     = anchorPosition->vx - (partToWorld.t[0] - rootToWorld.t[0]);
    parts[0].coord.t[2]     = anchorPosition->vz - (partToWorld.t[2] - rootToWorld.t[2]);
    parts[0].composeStamp   = GRAPHICS_COORD_DIRTY;
    partCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(partCoord);
#if STALKER_ZEBRA_IVORY_PIN_RECOMPOSES_ROOT
    actorRenderComposeCoord(parts);
#endif
}
