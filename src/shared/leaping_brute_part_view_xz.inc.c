/* Part of the leaping brute library; see leaping_brute.h. */

/// Refreshes the view coordinate and coordinate `index` of the actor's model,
/// then stores that coordinate's view-space X and Z translation to `out`; `y`
/// is left untouched. Every caller passes the work block's `field_88`.
void bruteReadPartViewXZ(Task* task, s16 index, BruteViewPos* out)
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
    Gp_WorldToLocal(&gGfxViewCoord.workm, &coord->workm, &local);
    out->x              = local.t[0];
    out->z              = local.t[2];
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}
