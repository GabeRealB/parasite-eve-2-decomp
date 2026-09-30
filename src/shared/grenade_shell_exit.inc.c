/* Part of the grenade shell library; see grenade_shell.h. */

/// Exit callback: unlinks both collision nodes the spawn state linked and kills
/// the task.
void grenadeShellExit(Task* task)
{
    WeaponGrenadeWork* work = task->work;

    Gp_UnlinkObj(&work->obj);
    Gp_UnlinkObj(&work->obj2);
    taskKill(task);
}
