/* Part of the glow pod library; see glow_pod.h. */

/// Exit callback of the second enemy: detaches the enemy's contact records,
/// unlinks its node and the work's three bodies, then runs the common enemy
/// task exit.
void glowPodExit(Task* task)
{
    GlowPodWork* work;
    GpEnemy*     enemy;

    enemy = task->spawnArg2.pointer;
    work  = (GlowPodWork*)task->work;

    enemy->recs = 0;
    Gp_UnlinkNode(&enemy->node);
    Gp_UnlinkObj(&work->field_14C);
    Gp_UnlinkObj(&work->field_FC);
    Gp_UnlinkObj(&work->field_184);
    Gp_EnemyTaskExit(task);
}
