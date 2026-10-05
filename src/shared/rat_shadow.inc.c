/* Part of the Rat library; see rat.h. */

/// Draws a ground quad (size 0x1C0, 0x80) under the model root's world
/// position.
void ratShadow(Task* arg0)
{
    GfxCoord* coord;
    VECTOR3   vec;

    coord  = arg0->extra.tmd->coords;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    effectDrawGroundShadow(&vec, 0x1C0, 0x80);
}
