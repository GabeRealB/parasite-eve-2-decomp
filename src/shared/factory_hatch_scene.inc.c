/* Part of the factory lift library; see factory_lift.h. */

void factoryHatchScene(Task* task)
{
    enum {
        FACTORY_HATCH_SCENE_OPEN            = 0,
        FACTORY_HATCH_SCENE_WAIT_OPEN       = 1,
        FACTORY_HATCH_SCENE_RUN_CAP         = 2,
        FACTORY_HATCH_SCENE_WAIT_CLOSE      = 3,
        FACTORY_HATCH_SCENE_OPEN_COUNTDOWN  = 60,
        FACTORY_HATCH_SCENE_CLOSE_COUNTDOWN = 30,
    };
    switch (task->state) {
        case FACTORY_HATCH_SCENE_OPEN:
            gameFlagSetNibble(GAME_FLAG_FACTORY_HATCH_OPEN, 1);
            task->killCountdown = FACTORY_HATCH_SCENE_OPEN_COUNTDOWN;
            task->state         = task->state + 1;
            return;
        case FACTORY_HATCH_SCENE_RUN_CAP:
            capRunCommandWithTransition(task->spawnArg1.value);
            gameFlagSetNibble(GAME_FLAG_FACTORY_HATCH_OPEN, 0);
            task->killCountdown = FACTORY_HATCH_SCENE_CLOSE_COUNTDOWN;
            task->state         = task->state + 1;
            return;
        case FACTORY_HATCH_SCENE_WAIT_OPEN:
        case FACTORY_HATCH_SCENE_WAIT_CLOSE:
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
