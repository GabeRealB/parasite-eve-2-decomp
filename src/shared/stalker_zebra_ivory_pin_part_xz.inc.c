/* Part of the Ivory/Zebra Stalker library; see stalker_zebra_ivory.h. */

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
#if !STALKER_ZEBRA_IVORY_PIN_UPDATES_ROOT
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
#if STALKER_ZEBRA_IVORY_PIN_UPDATES_ROOT
    actorRenderComposeCoord(parts);
#endif
}
