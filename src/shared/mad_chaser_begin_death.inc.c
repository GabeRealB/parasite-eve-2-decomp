/* Part of the Mad Chaser library; see mad_chaser.h. */

/// First step of the death squash: drops the enemy's contact records, unlinks
/// the three collision spheres and snapshots the root matrix with the Y scale
/// `shrinkScaleY` at 0x1000, then switches the model to light mode 1, clears the
/// frame counter and advances the state.
void madChaserBeginDeath(Task* arg0)
{
    GfxCoord*      coord = arg0->extra.tmd->coords;
    Enemy*         enemy = (Enemy*)arg0->spawnArg2.pointer;
    MadChaserWork* work  = (MadChaserWork*)arg0->work;
    MadChaserWork* objWork;

    enemy->recs = 0;

    objWork = (MadChaserWork*)arg0->work;
    worldCollisionUnlinkBody(&objWork->pairBody);
    worldCollisionUnlinkBody(&objWork->gridBody);
    worldCollisionUnlinkBody(&objWork->attackBody);

    work->shrinkScaleY = 0x1000;
    work->savedRootMtx = coord->coord;

    Gp_SetLightMode(arg0->spawnArg2.pointer, ENEMY_COLOR_WEIGHTED);

    work->stateFrames = 0;
    work->state++;
}
