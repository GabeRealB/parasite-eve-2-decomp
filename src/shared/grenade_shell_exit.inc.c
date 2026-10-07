/* Part of the grenade shell library; see grenade_shell.h. */

/// Removes a grenade shell's two collision bodies and releases its task.
///
/// Requires live `WeaponGrenadeWork` owned by `task`, with its sphere and
/// capsule initialized by the spawn state. Unlinks both before `taskKill`
/// releases work and schedules or performs body/task teardown. Used as both
/// an exit callback and the terminal projectile state; the caller must not
/// reuse the released work and must honor `taskKill`'s task-lifetime contract.
static void _grenadeShellExit(Task* task)
{
    WeaponGrenadeWork* work = task->work;

    worldCollisionUnlinkBody(&work->sphereBody);
    worldCollisionUnlinkBody(&work->capsuleBody);
    taskKill(task);
}
