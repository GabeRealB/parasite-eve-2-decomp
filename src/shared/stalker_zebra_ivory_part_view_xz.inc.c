/* Part of the Zebra and Ivory Stalker library; see stalker_zebra_ivory.h. */

/// Refreshes the view coordinate and coordinate `index` of the actor's model,
/// then stores that coordinate's view-space X and Z translation to `out`; `vy`
/// is left untouched. Every caller passes the work block's `anchorPos`.
void stalkerZebraIvoryReadPartViewXZ(Task* task, s16 index, SVECTOR3* out)
{
    MATRIX    local;
    GfxCoord* coord;
    GfxCoord* coords;

    coords                     = task->extra.tmd->coords;
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    coord                      = &coords[index];
    Gp_UpdateCoord(&gGfxViewCoord);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(coord);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coord->workm, &local);
    out->vx             = local.t[0];
    out->vz             = local.t[2];
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}
