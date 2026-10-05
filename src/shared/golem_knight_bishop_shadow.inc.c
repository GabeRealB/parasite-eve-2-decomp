/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Draws the ground shadow quad, 0x300 across, under the fourth part's
/// horizontal position at the root's height, shaded by `shadowShade` - which a
/// zero turns into -1 first, so a shadow nothing has raised is not drawn.
void golemKnightBishopDrawShadow(Task* arg0)
{
    GolemKnightBishopWork* work;
    GfxCoord*              coord;
    GfxCoord*              sub;
    VECTOR3                vec;

    work  = arg0->work;
    coord = &arg0->extra.tmd->coords[0];
    sub   = &arg0->extra.tmd->coords[3];
    if (work->shadowShade == 0) {
        work->shadowShade = -1;
    }
    vec.vx = sub->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = sub->workm.t[2];
    effectDrawGroundShadow(&vec, 0x300, work->shadowShade);
}
