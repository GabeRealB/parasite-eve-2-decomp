/* Part of the Glutton library; see glutton.h. */

/// Releases the host enemy after sending its surviving escorts to teardown.
///
/// When host work exists, requests state 2 on every surviving escort, unlinks
/// hit bodies 0, 1 and 3..8, and detaches the enemy contact table. Group 2 is
/// not unlinked here; its teardown ownership is unproven.
/// Requires the task's live enemy in `spawnArg2.pointer`. `enemyDestroy` releases
/// that enemy and starts task teardown; neither may be accessed afterwards.
static void _gluttonExit(Task* task)
{
    enum { GLUTTON_ESCORT_TEARDOWN_STATE = 2 };
    GluttonWork* work;
    Enemy*       enemy;
    s16          escortIndex;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work != NULL) {
        for (escortIndex = 0; escortIndex < ARRAY_SIZE(work->escorts); escortIndex++) {
            if (work->escorts[escortIndex] != NULL) {
                work->escorts[escortIndex]->task->state = GLUTTON_ESCORT_TEARDOWN_STATE;
            }
        }
        worldCollisionUnlinkBody(&work->hits[0].body);
        worldCollisionUnlinkBody(&work->hits[1].body);
        worldCollisionUnlinkBody(&work->hits[3].body);
        worldCollisionUnlinkBody(&work->hits[4].body);
        worldCollisionUnlinkBody(&work->hits[5].body);
        worldCollisionUnlinkBody(&work->hits[6].body);
        worldCollisionUnlinkBody(&work->hits[7].body);
        worldCollisionUnlinkBody(&work->hits[8].body);
        enemy->recs = NULL;
    }
    enemyDestroy(enemy, task);
}
