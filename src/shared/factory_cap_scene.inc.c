/* Part of the factory lift library; see factory_lift.h. */

/// Runs the cap command in `Task::spawnArg1` unless game flag 0x47 is set, then
/// kills the task.
void factoryCapScene(Task* arg0)
{
    if (gameFlagGetNibble(GAME_FLAG_FACTORY_BARRIER_CLEARED) == 0) {
        capRunCommandWithTransition(arg0->spawnArg1.value);
    }
    taskKill(arg0);
}
