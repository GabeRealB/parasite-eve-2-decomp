/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Unlinks the burst corpse's three collision bodies without releasing storage.
///
/// bodyWork must be live with valid links for any linked body. Detaches pair,
/// grid and attack bodies in order; their shapes and contact arrays stay owned
/// by the task until despawn releases its work block.
static __inline__ void _madChaserBurstUnlinkBodies(MadChaserWork* bodyWork)
{
    worldCollisionUnlinkBody(&bodyWork->pairBody);
    worldCollisionUnlinkBody(&bodyWork->gridBody);
    worldCollisionUnlinkBody(&bodyWork->attackBody);
}

/// Bursts the dead body into effects and hands its task to despawn.
///
/// Requires a live enemy/model/work block and an acquired battle reference.
/// Releases the primitive buffer and suppresses its automatic reallocation,
/// spawns gibs, then releases the battle reference with rewards. Detaches the
/// enemy's borrowed contacts and unlinks pair/grid/attack bodies in that order.
/// Enters despawn with behavior and sub-state zero; model and work allocations
/// remain task-owned for that later teardown.
static void _madChaserBurst(Task* task)
{
    MadChaserWork* work;
    MadChaserWork* resetWork;
    TmdObject*     model;
    Enemy*         enemy;

    model = task->extra.tmd;
    enemy = task->spawnArg2.pointer;
    tmdFreePrimitiveBuffer(model);
    model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    _madChaserSpawnGibs(task);
    sceneReleaseBattleRefWithRewards(task, 0);
    enemy->recs = NULL;
    work        = task->work;
    _madChaserBurstUnlinkBodies(work);
    resetWork           = task->work;
    task->state         = MAD_CHASER_TASK_DESPAWN;
    resetWork->state    = 0;
    resetWork->subState = 0;
}
