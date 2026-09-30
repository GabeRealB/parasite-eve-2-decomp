/* Part of the factory lift library; see factory_lift.h. */

/// Kills the task; the factory model's exit callback.
void factoryLiftExit(Task* task)
{
    taskKill(task);
}
