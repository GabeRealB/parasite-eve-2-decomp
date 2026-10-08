/* Part of the Rat library; see rat.h. */

/// Updates the rat's lighting and color from its cached root translation.
///
/// Requires a live model/`Enemy` with writable light/color matrices and a cache
/// suitable for the lighting query. Only XYZ are supplied; this function does
/// not compose the root or retain the temporary sample.
static void _ratUpdateColor(Task* actor)
{
    GfxCoord* rootCoord;
    VECTOR3   samplePosition;

    rootCoord         = actor->extra.tmd->coords;
    samplePosition.vx = rootCoord->workm.t[0];
    samplePosition.vy = rootCoord->workm.t[1];
    samplePosition.vz = rootCoord->workm.t[2];
    worldCoordUpdateActorColor(actor->spawnArg2.pointer, &samplePosition, 0, 0);
}
