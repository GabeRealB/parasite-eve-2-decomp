/* Part of the factory lift library; see factory_lift.h. */

void factoryLampScene(Task* task)
{
    enum {
        FACTORY_LAMP_SCENE_START       = 0,
        FACTORY_LAMP_SCENE_HIDE_PLAYER = 1,
        FACTORY_LAMP_SCENE_WAIT_CAP    = 2,
        FACTORY_LAMP_SCENE_FINISH      = 3,
        FACTORY_LAMP_PROGRESS_COMPLETE = 2,
    };
    s32 state = task->state;

    switch (state) {
        case FACTORY_LAMP_SCENE_START:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            companionSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            capRunCommand(task->spawnArg1.value, CAP_PLAYBACK_IN_PLACE);
            task->state++;
            return;
        case FACTORY_LAMP_SCENE_HIDE_PLAYER:
            if (gameFlagGetNibble(GAME_FLAG_FACTORY_LAMP_PROGRESS) < FACTORY_LAMP_PROGRESS_COMPLETE) {
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, 0, 0);
            }
            task->state++;
            /* fallthrough */
        case FACTORY_LAMP_SCENE_WAIT_CAP:
            if (capIsBusy() != 0) {
                return;
            }
            task->state++;
            return;
        case FACTORY_LAMP_SCENE_FINISH:
            if (capGetVariantKey() == state) {
                gameFlagSetNibble(GAME_FLAG_FACTORY_LAMP_PROGRESS, FACTORY_LAMP_PROGRESS_COMPLETE);
            }
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            companionSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
            taskKill(task);
            break;
    }
}
