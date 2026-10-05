/* Part of the grenade shell library; see grenade_shell.h. */

/// Exit callback: unlinks both collision nodes the spawn state linked and kills
/// the task.
void grenadeShellExit(Task* task)
{
    WeaponGrenadeWork* work = task->work;

    worldCollisionUnlinkBody(&work->sphereBody);
    worldCollisionUnlinkBody(&work->capsuleBody);
    taskKill(task);
}
