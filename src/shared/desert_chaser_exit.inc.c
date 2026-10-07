/* Part of the Desert Chaser library; see desert_chaser.h. */

/// Releases the enemy record attached to the exiting task.
///
/// Installed as the cutscene and regular builds' exit callback; the enemy
/// teardown owns unlinking and releasing that record and the task's work.
static void _desertChaserExit(Task* task)
{
    enemyDestroy(task->spawnArg2.pointer, task);
}
