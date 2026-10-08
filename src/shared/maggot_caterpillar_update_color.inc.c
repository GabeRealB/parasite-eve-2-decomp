/* Part of the Maggot/Caterpillar library; see maggot_caterpillar.h. */

/// Updates enemy lighting and colour from the root's cached translation.
///
/// Requires a live model, writable lighting matrices and the owning enemy in
/// `spawnArg2.pointer`. Copies signed-word XYZ without composing or converting
/// the root; the lighting query interprets the sample as world coordinates.
/// The sample is temporary and no pointer to it is retained.
static void _maggotCaterpillarUpdateColor(Task* actor)
{
    GfxCoord* coord;
    VECTOR    colorPosition;

    coord            = actor->extra.tmd->coords;
    colorPosition.vx = coord->workm.t[0];
    colorPosition.vy = coord->workm.t[1];
    colorPosition.vz = coord->workm.t[2];
    worldCoordUpdateActorColor(actor->spawnArg2.pointer, &colorPosition, 0, 0);
}
