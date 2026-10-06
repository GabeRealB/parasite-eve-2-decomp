/* Part of the Maggot/Caterpillar library; see maggot_caterpillar.h. */

/// Passes the world position of the model's root coordinate to
/// `worldCoordUpdateActorColor` for the context, with both trailing arguments 0.
void maggotCaterpillarUpdateColor(Task* arg0)
{
    GfxCoord* coord;
    VECTOR    vec;

    coord  = arg0->extra.tmd->coords;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    worldCoordUpdateActorColor(arg0->spawnArg2.pointer, &vec, 0, 0);
}
