/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Translates the model root to keep one part at a world-space anchor.
///
/// Requires a live model with 0 <= partIndex < partCount and its root parented
/// directly to the view coordinate. worldAnchor supplies signed-halfword XYZ
/// in whole game-coordinate units; only those six bytes are read, and pad is
/// ignored. Borrows the anchor for the call. Composes the part, removes the view
/// transform from both part and root, and subtracts their world-space offset
/// from the anchor. Writes full-width root XYZ and dirties the selected part;
/// the caller invalidates the root before its next composition.
static void _madChaserPinPart(Task* task, s16 partIndex, const SVECTOR* worldAnchor)
{
    MATRIX    rootToWorld;
    MATRIX    partToWorld;
    GfxCoord* partCoord;
    GfxCoord* coords;

    coords    = task->extra.tmd->coords;
    partCoord = &coords[partIndex];
    actorRenderComposeCoord(partCoord);
    // Remove the view so the offset and anchor share the root's parent frame.
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coords->workm, &rootToWorld);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &partCoord->workm, &partToWorld);
    coords->coord.t[0]      = worldAnchor->vx - (partToWorld.t[0] - rootToWorld.t[0]);
    coords->coord.t[1]      = worldAnchor->vy - (partToWorld.t[1] - rootToWorld.t[1]);
    coords->coord.t[2]      = worldAnchor->vz - (partToWorld.t[2] - rootToWorld.t[2]);
    partCoord->composeStamp = GRAPHICS_COORD_DIRTY;
}
