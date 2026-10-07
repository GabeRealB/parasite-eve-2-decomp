/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Removes the three task-owned collision bodies from their active lists.
///
/// The work block and body storage remain live; contact arrays are retained.
static __inline__ void _madChaserDeathStartShrinkUnlinkBodies(MadChaserWork* bodyWork)
{
    worldCollisionUnlinkBody(&bodyWork->pairBody);
    worldCollisionUnlinkBody(&bodyWork->gridBody);
    worldCollisionUnlinkBody(&bodyWork->attackBody);
}

/// Starts the ordinary-death shrink after the settle animation reaches a boundary.
///
/// Requires a live enemy, model root and task-owned Mad Chaser work. Detaches
/// the enemy's contact records and unlinks all three collision bodies, retaining
/// their storage. Saves the complete local root matrix for subsequent shrink
/// frames and starts at Q12 Y scale 1.0 with weighted colour. Clears stateFrames
/// and advances the death behavior from 3 to 4; resources remain task-owned.
static void _madChaserDeathStartShrink(Task* task)
{
    GfxCoord*      root  = task->extra.tmd->coords;
    Enemy*         enemy = task->spawnArg2.pointer;
    MadChaserWork* work  = task->work;
    MadChaserWork* bodyWork;

    enemy->recs = NULL;

    bodyWork = task->work;
    _madChaserDeathStartShrinkUnlinkBodies(bodyWork);

    work->shrinkScaleY = ONE;
    work->savedRootMtx = root->coord;

    worldCoordSetActorColorMode(task->spawnArg2.pointer, ENEMY_COLOR_WEIGHTED);

    work->stateFrames = 0;
    work->state++;
}
