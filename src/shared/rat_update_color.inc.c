/* Part of the Rat library; see rat.h. */

/// Updates the enemy's lighting colour from the world position of the model
/// root. (Byte-identical to _mothUpdateColor and to the same helper in about a
/// dozen other actor packages.)
void ratUpdateColor(Task* arg0)
{
    GfxCoord* coord;
    VECTOR    vec;

    coord  = arg0->extra.tmd->coords;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    worldCoordUpdateActorColor(arg0->spawnArg2.pointer, &vec, 0, 0);
}
