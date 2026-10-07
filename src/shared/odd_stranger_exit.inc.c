/* Part of the Odd Stranger library; see odd_stranger.h. */

/// Releases the Odd Stranger's child tasks, collision links and enemy task.
///
/// `task` and its enemy spawn argument must be live. Work may be NULL during
/// partial initialization; otherwise detach all three bodies and clear the
/// enemy's borrowed contacts before `enemyDestroy` releases the task.
static void _oddStrangerExit(Task* task)
{
    OddStrangerWork* work;
    Enemy*           enemy;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work != NULL) {
        if (work->childTask0 != NULL) {
            taskKill(work->childTask0);
        }
        if (work->childTask1 != NULL) {
            taskKill(work->childTask1);
        }
        worldCollisionUnlinkBody(&work->attackBody);
        worldCollisionUnlinkBody(&work->hitBody);
        worldCollisionUnlinkBody(&work->gridBody);
        enemy->recs = NULL;
    }
    enemyDestroy(enemy, task);
}
