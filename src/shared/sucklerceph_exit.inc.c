/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Exit callback of the first enemy: detaches the enemy's contact records,
/// unlinks its node and the work's four bodies, then runs the common enemy
/// task exit.
void sucklercephExit(Task* task)
{
    SucklercephWork* work;
    Enemy*           enemy;

    enemy = task->spawnArg2.pointer;
    work  = (SucklercephWork*)task->work;

    enemy->recs = 0;
    worldTargetUnlinkNode(&enemy->node);
    Gp_UnlinkObj(&work->objFC);
    Gp_UnlinkObj(&work->obj134);
    Gp_UnlinkObj(&work->obj1B4);
    Gp_UnlinkObj(&work->obj1EC);
    Gp_EnemyTaskExit(task);
}
