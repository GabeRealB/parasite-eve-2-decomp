/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Exit callback of the first enemy: detaches the enemy's contact records,
/// unlinks its node and the work's four bodies, then runs the common enemy
/// task exit.
void sucklercephExit(Task* task)
{
    SucklercephWork* work;
    Enemy*           enemy;

    enemy = task->spawnArg2.pointer;
    work  = task->work;

    enemy->recs = 0;
    worldTargetUnlinkNode(&enemy->node);
    worldCollisionUnlinkBody(&work->senseBody);
    worldCollisionUnlinkBody(&work->body);
    worldCollisionUnlinkBody(&work->attackBody);
    worldCollisionUnlinkBody(&work->blastBody);
    enemyTaskExit(task);
}
