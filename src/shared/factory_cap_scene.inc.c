/* Part of the factory lift library; see factory_lift.h. */

void factoryCapScene(Task* task)
{
    if (gameFlagGetNibble(GAME_FLAG_FACTORY_BARRIER_CLEARED) == 0) {
        capRunCommandWithTransition(task->spawnArg1.value);
    }
    taskKill(task);
}
