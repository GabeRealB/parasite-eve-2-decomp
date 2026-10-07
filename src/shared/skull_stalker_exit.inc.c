/* Part of the Skull Stalker library; see skull_stalker.h. */

/// Detaches targeting and collision records before releasing the enemy task.
///
/// The task must retain its enemy, allocated work and model. Unlinking also
/// tolerates bodies already detached by the death state. Common enemy teardown
/// frees the enemy and work and starts body/task teardown after these embedded
/// records leave their lists.
static void _skullStalkerExit(Task* task)
{
    SkullStalkerWork* work;
    Enemy*            enemy;

    enemy = task->spawnArg2.pointer;
    work  = task->work;

    enemy->recs = 0;
    worldTargetUnlinkNode(&enemy->node);
    worldCollisionUnlinkBody(&work->senseBody);
    worldCollisionUnlinkBody(&work->frontSenseBody);
    worldCollisionUnlinkBody(&work->body);
    enemyTaskExit(task);
}
