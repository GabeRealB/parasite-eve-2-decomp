/* Part of the Odd Stranger library; see odd_stranger.h. */

/// `Task::exitCallback` teardown: kills the two helper tasks, unlinks the three
/// display nodes and drops the enemy's `recs`, then destroys the enemy.
void oddStrangerExit(Task* task)
{
    OddStrangerWork* work;
    Enemy*           enemy;

    work  = task->work;
    enemy = (Enemy*)task->spawnArg2.pointer;
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
        enemy->recs = 0;
    }
    enemyDestroy(enemy, task);
}
