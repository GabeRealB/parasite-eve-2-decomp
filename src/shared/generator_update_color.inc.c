/* Part of the Generator library; see generator.h. */

/// Updates body lighting and color at its root's cached translation.
///
/// Requires a live Enemy/model with writable light and color matrices and a
/// composed root cache. Passes its three cached translation components without
/// refreshing composition. No pointer to the temporary VECTOR is retained.
static void _generatorUpdateColor(Task* task)
{
    GfxCoord* coord;
    VECTOR    samplePosition;

    coord             = task->extra.tmd->coords;
    samplePosition.vx = coord->workm.t[0];
    samplePosition.vy = coord->workm.t[1];
    samplePosition.vz = coord->workm.t[2];
    worldCoordUpdateActorColor(task->spawnArg2.pointer, &samplePosition, 0, 0);
}
