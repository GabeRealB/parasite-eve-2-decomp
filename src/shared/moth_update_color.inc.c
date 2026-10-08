/* Part of the Moth library; see moth.h. */

/// Updates moth lighting and colour from the model root's cached translation.
///
/// Requires the live enemy in spawnArg2, model lighting matrices and a valid
/// root composition cache. Copies XYZ into a temporary VECTOR for the world
/// lighting query; does not compose or convert the coordinate. The query reads
/// only XYZ and retains neither the vector nor any scratch storage.
static void _mothUpdateColor(Task* task)
{
    GfxCoord* rootCoord;
    VECTOR    samplePosition;

    rootCoord         = task->extra.tmd->coords;
    samplePosition.vx = rootCoord->workm.t[0];
    samplePosition.vy = rootCoord->workm.t[1];
    samplePosition.vz = rootCoord->workm.t[2];
    worldCoordUpdateActorColor(task->spawnArg2.pointer, &samplePosition, 0, 0);
}
