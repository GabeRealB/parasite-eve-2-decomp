/* Part of the Generator library; see generator.h. */

/// Hands the model's world position (its coordinate's `workm` translation) to
/// `worldCoordUpdateActorColor` for the enemy, with zero for the unused arguments.
void generatorUpdateColor(Task* arg0)
{
    GfxCoord* coord;
    VECTOR    vec;

    coord  = arg0->extra.tmd->coords;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    worldCoordUpdateActorColor(arg0->spawnArg2.pointer, &vec, 0, 0);
}
