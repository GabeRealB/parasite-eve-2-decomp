/* Part of the No. 9 GOLEM library; see no9_golem.h. */

/// Draws the actor's ground shadow: a 0x300-wide quad at shade 0x80, placed at
/// the second coordinate's x/z and the first coordinate's y, so it lies on the
/// ground under the body even when the two coordinates are apart.
void no9GolemDrawShadow(Task* arg0)
{
    GfxCoord* coord;
    GfxCoord* sub;
    VECTOR3   vec;

    coord  = arg0->extra.tmd->coords;
    sub    = &arg0->extra.tmd->coords[1];
    vec.vx = sub->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = sub->workm.t[2];
    effectDrawGroundShadow(&vec, 0x300, 0x80);
}
