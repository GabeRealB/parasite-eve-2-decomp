/* Part of the factory lift library; see factory_lift.h. */

/// Cutscene driver for the factory room: silences both weapons, runs the cap
/// (cutscene) command in `Task::spawnArg1`, then waits for the cap to report
/// event key 3 before setting the two progress flags and starting the follow-up
/// cap slot. Any state past 4 restores the weapons and kills the task.
void factoryPowerScene(Task* task)
{
    switch (task->state) {
        case 0:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            companionSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            capRunCommand(task->spawnArg1.value, CAP_PLAYBACK_IN_PLACE);
            task->state++;
            return;
        case 1:
            if (gameFlagGetNibble(GAME_FLAG_FACTORY_POWER_ON) <= 0) {
                if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
                    factoryDayShowView11Sprite(0);
                } else {
                    factoryNightShowView11Sprite(0);
                }
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, 0, 0);
            }
            task->state++;
            /* fallthrough */
        case 2:
            if (capIsBusy() != 0) {
                return;
            }
            /* fallthrough */
        case 3:
            if (capGetVariantKey() == 3) {
                gameFlagSetNibble(GAME_FLAG_FACTORY_POWER_ON, 1);
                gameFlagSetNibble(GAME_FLAG_FACTORY_LAMP_PROGRESS, 1);
                if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
                    factoryDayShowView11Sprite(1);
                    factoryDayShowView9Sprite(1);
                } else {
                    factoryNightShowView11Sprite(1);
                    factoryNightShowView9Sprite(1);
                }
                capStartSequenceSlot(task->spawnArg1.value, 1, 2);
            }
            task->state++;
            return;
        case 4:
            if (capIsBusy() != 0) {
                return;
            }
            /* fallthrough */
        default:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            companionSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
            taskKill(task);
            break;
    }
}
