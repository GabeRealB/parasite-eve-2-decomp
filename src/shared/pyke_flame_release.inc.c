/* Part of the Pyke flame library; see pyke_flame.h. */

/// Exit callback: unlinks the flame's collision body leading `Task::work`, if
/// one was linked, and releases the `EffectWork` in `Task::spawnArg2`.
static void pykeFlameRelease(Task* task)
{
    WorldCollisionBody* obj = task->work;
    void*               mem = task->spawnArg2.pointer;

    if (obj != NULL) {
        Gp_UnlinkObj(obj);
    }
    effectKillTask(mem, task);
}
