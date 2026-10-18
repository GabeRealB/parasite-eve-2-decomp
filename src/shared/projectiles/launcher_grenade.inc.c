/* Part of the grenade shell library; see launcher_grenade.h. */

#include "types.h"

#include "gameplay/geometry.h"
#include "gameplay/world_collision.h"

#include "main/task.h"
#include "main/task_types.h"

#include "weapons/weapon.h"

#include "launcher_grenade.h"

/// Counts down the grenade's live blast sphere before advancing to teardown.
///
/// Entered in task state 2 with live `WeaponGrenadeWork`. Detonation has changed
/// the flight timer into a whole-frame countdown (8 normally, 1 for the airburst
/// round). Decrements before testing; zero or negative advances to state 3.
/// Collision bodies stay linked until the teardown handler runs.
static void _launcherGrenadeBlast(Task* task)
{
    enum { LAUNCHER_GRENADE_TASK_STATE_EXIT = 3 };
    WeaponGrenadeWork* work  = task->work;
    s32                timer = work->flightTimer.word - 1;

    work->flightTimer.word = timer;
    if (timer <= 0) {
        task->state = LAUNCHER_GRENADE_TASK_STATE_EXIT;
    }
}

/// Removes a grenade shell's two collision bodies and releases its task.
///
/// Requires live `WeaponGrenadeWork` owned by `task`, with its sphere and
/// capsule initialized by the spawn state. Unlinks both before `taskKill`
/// releases work and schedules or performs body/task teardown. Used as both
/// an exit callback and the terminal projectile state; the caller must not
/// reuse the released work and must honor `taskKill`'s task-lifetime contract.
static void _launcherGrenadeExit(Task* task)
{
    WeaponGrenadeWork* work = task->work;

    worldCollisionUnlinkBody(&work->sphereBody);
    worldCollisionUnlinkBody(&work->capsuleBody);
    taskKill(task);
}
