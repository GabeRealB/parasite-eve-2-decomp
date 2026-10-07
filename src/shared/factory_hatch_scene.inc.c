/* Part of the factory lift library; see factory_lift.h. */

/// Plays the cutscene sequence out: sets game flag 0x4E, waits 0x3C frames,
/// runs the cap command in `Task::spawnArg1` and clears the flag again, waits
/// 0x1E frames, then hands the player and the ally their weapons back and kills
/// the task.
void factoryHatchScene(Task* task)
{
    switch (task->state) {
        case 0:
            gameFlagSetNibble(GAME_FLAG_FACTORY_HATCH_OPEN, 1);
            task->killCountdown = 0x3C;
            task->state         = task->state + 1;
            return;
        case 2:
            capRunCommandWithTransition(task->spawnArg1.value);
            gameFlagSetNibble(GAME_FLAG_FACTORY_HATCH_OPEN, 0);
            task->killCountdown = 0x1E;
            task->state         = task->state + 1;
            return;
        case 1:
        case 3:
            if (--task->killCountdown < 0) {
                task->state = task->state + 1;
            }
            return;
        default:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            companionSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            taskKill(task);
            return;
    }
}
