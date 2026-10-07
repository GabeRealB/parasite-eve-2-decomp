/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Unlinks the three retained collision bodies used by command shrink-death.
static __inline__ void _madChaserBeginShrinkUnlinkBodies(MadChaserWork* bodyWork)
{
    worldCollisionUnlinkBody(&bodyWork->pairBody);
    worldCollisionUnlinkBody(&bodyWork->gridBody);
    worldCollisionUnlinkBody(&bodyWork->attackBody);
}

/// Saves the root transform and starts command shrink-death at full Y scale.
///
/// Requires a live enemy, model root and task-owned Mad Chaser work in command
/// shrink-death behavior 3. Detaches contact records and unlinks all three
/// collision bodies, retaining their storage. Saves the complete local root
/// matrix for subsequent shrink frames, sets Q12 Y scale 1.0 and selects weighted
/// colour. Clears stateFrames and advances to behavior 4; resources stay live.
static void _madChaserBeginShrink(Task* task)
{
    Enemy*         enemy = task->spawnArg2.pointer;
    MadChaserWork* work  = task->work;
    GfxCoord*      root  = task->extra.tmd->coords;
    MadChaserWork* bodyWork;

    enemy->recs = NULL;

    bodyWork = task->work;
    _madChaserBeginShrinkUnlinkBodies(bodyWork);

    work->shrinkScaleY = ONE;
    work->savedRootMtx = root->coord;

    worldCoordSetActorColorMode(task->spawnArg2.pointer, ENEMY_COLOR_WEIGHTED);

    work->stateFrames = 0;
    work->state++;
}
