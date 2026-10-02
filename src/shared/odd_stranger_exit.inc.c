/* Part of the Odd Stranger library; see odd_stranger.h. */

/// `Task::exitCallback` teardown: kills the two helper tasks, unlinks the three
/// display nodes and drops the enemy's `recs`, then destroys the enemy.
void oddStrangerExit(Task* task)
{
    OddStrangerWork* work;
    Enemy*           enemy;

    work  = (OddStrangerWork*)task->work;
    enemy = (Enemy*)task->spawnArg2.pointer;
    if (work != NULL) {
        if (work->field_C1C != NULL) {
            taskKill(work->field_C1C);
        }
        if (work->field_C20 != NULL) {
            taskKill(work->field_C20);
        }
        Gp_UnlinkObj(&work->field_B50);
        Gp_UnlinkObj(&work->field_8D0);
        Gp_UnlinkObj(&work->field_A10);
        enemy->recs = 0;
    }
    enemyDestroy(enemy, task);
}
