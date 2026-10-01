/* Part of the Desert Chaser library; see desert_chaser.h. */

/// `Task::exitCallback` the spawn handler installs: destroys the enemy the
/// task carries.
void desertChaserExit(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2.pointer, task);
}
