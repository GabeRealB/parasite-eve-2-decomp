/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Starts the death shrink, the same body as `madChaserBeginDeath`:
/// detaches the records, unlinks the three hit bodies, sets the Y scale to
/// 1.0, saves the root matrix, sets light mode 1 and advances the state.
void madChaserBeginShrink(Task* task)
{
    Enemy*         enemy = (Enemy*)task->spawnArg2.pointer;
    MadChaserWork* work  = (MadChaserWork*)task->work;
    GfxCoord*      coord = task->extra.tmd->coords;
    MadChaserWork* objWork;

    enemy->recs = 0;

    objWork = (MadChaserWork*)task->work;
    worldCollisionUnlinkBody(&objWork->pairBody);
    worldCollisionUnlinkBody(&objWork->gridBody);
    worldCollisionUnlinkBody(&objWork->attackBody);

    work->shrinkScaleY = 0x1000;
    work->savedRootMtx = coord->coord;

    worldCoordSetActorColorMode(task->spawnArg2.pointer, ENEMY_COLOR_WEIGHTED);

    work->stateFrames = 0;
    work->state++;
}
