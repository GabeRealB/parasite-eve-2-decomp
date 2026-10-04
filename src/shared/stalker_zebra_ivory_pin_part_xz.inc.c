/* Part of the Ivory/Zebra Stalker library; see stalker_zebra_ivory.h. */

/// Moves the root so that part `arg1` lands on the view-space X/Z position
/// `arg2`, measuring both through the view coordinate.
void stalkerZebraIvoryPinPartXZ(Task* arg0, s16 arg1, SVECTOR3* arg2)
{
    MATRIX    root;
    MATRIX    local;
    GfxCoord* coord;
    GfxCoord* coords;

    coords                     = arg0->extra.tmd->coords;
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    coord                      = &coords[arg1];
    actorRenderComposeCoord(&gGfxViewCoord);
#if !STALKER_ZEBRA_IVORY_PIN_UPDATES_ROOT
    coords[0].composeStamp = GRAPHICS_COORD_DIRTY;
#endif
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coords[0].workm, &root);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coord->workm, &local);
    coords[0].coord.t[0]   = arg2->vx - (local.t[0] - root.t[0]);
    coords[0].coord.t[2]   = arg2->vz - (local.t[2] - root.t[2]);
    coords[0].composeStamp = GRAPHICS_COORD_DIRTY;
    coord->composeStamp    = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
#if STALKER_ZEBRA_IVORY_PIN_UPDATES_ROOT
    actorRenderComposeCoord(coords);
#endif
}
