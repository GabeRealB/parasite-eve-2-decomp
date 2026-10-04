/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Moves the model so that part `part` lands on `pos`: sets the root
/// translation to `pos` less the part's view-space offset from the root, and
/// marks the part's coordinate dirty.
void madChaserPinPart(Task* arg0, s16 part, SVECTOR3* pos)
{
    MATRIX    local;
    MATRIX    world;
    GfxCoord* coord;
    GfxCoord* coords;

    coords = arg0->extra.tmd->coords;
    coord  = &coords[part];
    actorRenderComposeCoord(coord);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coords->workm, &local);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coord->workm, &world);
    coords->coord.t[0]  = pos->vx - (world.t[0] - local.t[0]);
    coords->coord.t[1]  = pos->vy - (world.t[1] - local.t[1]);
    coords->coord.t[2]  = pos->vz - (world.t[2] - local.t[2]);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}
