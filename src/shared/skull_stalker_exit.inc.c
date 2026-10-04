/* Part of the Skull Stalker library; see skull_stalker.h. */

/// Exit callback of the second enemy: detaches the enemy's contact records,
/// unlinks its node and the work's three bodies, then runs the common enemy
/// task exit.
void skullStalkerExit(Task* task)
{
    SkullStalkerWork* work;
    Enemy*            enemy;

    enemy = task->spawnArg2.pointer;
    work  = task->work;

    enemy->recs = 0;
    worldTargetUnlinkNode(&enemy->node);
    Gp_UnlinkObj(&work->senseBody);
    Gp_UnlinkObj(&work->frontSenseBody);
    Gp_UnlinkObj(&work->body);
    enemyTaskExit(task);
}
