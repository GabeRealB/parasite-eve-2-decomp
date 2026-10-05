/* Part of the Pyke flame library; see pyke_flame.h. */

/// Exit callback: unlinks the `PykeFlameBody` at `Task::work`, if one was
/// linked, and releases the `EffectWork` in `Task::spawnArg2`. `body` is the
/// block's first member, so the pointer is that `WorldCollisionBody`.
static void pykeFlameRelease(Task* task)
{
    WorldCollisionBody* body = task->work;
    void*               mem  = task->spawnArg2.pointer;

    if (body != NULL) {
        worldCollisionUnlinkBody(body);
    }
    effectKillTask(mem, task);
}
