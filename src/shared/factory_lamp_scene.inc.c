/* Part of the factory lift library; see factory_lift.h. */

/// Second cutscene driver for the night factory: silences both weapons, runs
/// the cap command in `Task::spawnArg1`, and once the cap reports event key 3
/// records progress flag 0x4A, restores the weapons and kills the task.
void factoryLampScene(Task* task)
{
    s32 state = task->state;

    switch (state) {
        case 0:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            companionSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            capRunCommand(task->spawnArg1.value, CAP_PLAYBACK_IN_PLACE);
            task->state++;
            return;
        case 1:
            if (gameFlagGetNibble(GAME_FLAG_FACTORY_LAMP_PROGRESS) < 2) {
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, 0, 0);
            }
            task->state++;
            /* fallthrough */
        case 2:
            if (capIsBusy() != 0) {
                return;
            }
            task->state++;
            return;
        case 3:
            if (capGetVariantKey() == state) {
                gameFlagSetNibble(GAME_FLAG_FACTORY_LAMP_PROGRESS, 2);
            }
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            companionSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
            taskKill(task);
            break;
    }
}
