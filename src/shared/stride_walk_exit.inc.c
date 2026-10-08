/* Part of the stride walk library; see stride_walk.h. */

/// Releases the stride walker's enemy through its task teardown callback.
///
/// `task->spawnArg2.pointer` must still hold the live Enemy supplied at spawn.
/// Enemy destruction detaches targeting and actor locks, frees the enemy and
/// kills the task; normal task teardown owns the work block and model resources.
/// The spawn argument is left dangling and must not be read after this call.
static void _strideWalkExit(Task* task)
{
    enemyDestroy(task->spawnArg2.pointer, task);
}
