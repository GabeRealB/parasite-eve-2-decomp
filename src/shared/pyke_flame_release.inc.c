/* Part of the Pyke flame library; see pyke_flame.h. */

/// Tears down a counted flying flame after unlinking its collision sphere.
///
/// `task` owns a `PykeFlameBody` in `work` (or NULL) and a
/// separate `EffectWork` in `spawnArg2.pointer`. Default task teardown frees
/// the collision block and releases the coordinate body and task. Call once;
/// neither allocation nor the task may be used after teardown.
static void _pykeFlameRelease(Task* task)
{
    PykeFlameBody* flame      = task->work;
    EffectWork*    effectWork = task->spawnArg2.pointer;

    if (flame != NULL) {
        worldCollisionUnlinkBody(&flame->body);
    }
    effectKillTask(effectWork, task);
}
