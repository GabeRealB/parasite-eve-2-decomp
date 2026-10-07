/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Detaches the Sucklerceph's collision and targeting state before teardown.
///
/// Requires the initialized task's live `SucklercephWork` and the owning enemy
/// in `spawnArg2.pointer`. Unlinking tolerates the bodies and target node already
/// being detached by the death state. Common enemy teardown releases the enemy
/// and task work; neither pointer remains live after the call.
static void _sucklercephExit(Task* task)
{
    SucklercephWork* work;
    Enemy*           enemy;

    enemy = task->spawnArg2.pointer;
    work  = task->work;

    // Detach borrowed storage and list links before either owner is freed.
    enemy->recs = NULL;
    worldTargetUnlinkNode(&enemy->node);
    worldCollisionUnlinkBody(&work->senseBody);
    worldCollisionUnlinkBody(&work->body);
    worldCollisionUnlinkBody(&work->attackBody);
    worldCollisionUnlinkBody(&work->blastBody);
    enemyTaskExit(task);
}
