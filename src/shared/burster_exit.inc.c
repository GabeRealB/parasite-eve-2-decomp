/* Part of the burster library; see burster.h. */

/// Exit callback of the first enemy: detaches the enemy's contact records,
/// unlinks its node and the work's four bodies, then runs the common enemy
/// task exit.
void bursterExit(Task* task)
{
    Actor104600Work* work;
    GpEnemy*         enemy;

    enemy = task->spawnArg2.pointer;
    work  = (Actor104600Work*)task->work;

    enemy->recs = 0;
    Gp_UnlinkNode(&enemy->node);
    Gp_UnlinkObj(&work->objFC);
    Gp_UnlinkObj(&work->obj134);
    Gp_UnlinkObj(&work->obj1B4);
    Gp_UnlinkObj(&work->obj1EC);
    Gp_EnemyTaskExit(task);
}
