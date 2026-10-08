/* Part of the factory lift library; see factory_lift.h. */

void factoryPowerScene(Task* task)
{
    enum {
        FACTORY_POWER_SCENE_START = 0,
        FACTORY_POWER_SCENE_HIDE_UNPOWERED,
        FACTORY_POWER_SCENE_WAIT_CAP,
        FACTORY_POWER_SCENE_APPLY_CHOICE,
        FACTORY_POWER_SCENE_WAIT_FOLLOWUP,
        FACTORY_POWER_CAP_ACCEPTED         = 3,
        FACTORY_POWER_FOLLOWUP_VARIANT_KEY = 2,
    };
    switch (task->state) {
        case FACTORY_POWER_SCENE_START:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            companionSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            capRunCommand(task->spawnArg1.value, CAP_PLAYBACK_IN_PLACE);
            task->state++;
            return;
        case FACTORY_POWER_SCENE_HIDE_UNPOWERED:
            if (gameFlagGetNibble(GAME_FLAG_FACTORY_POWER_ON) <= 0) {
                if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
                    dryfieldFactorySetView11SpriteVisible(false);
                } else {
                    dryfieldNightFactorySetView11SpriteVisible(false);
                }
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, 0, 0);
            }
            task->state++;
            /* fallthrough */
        case FACTORY_POWER_SCENE_WAIT_CAP:
            if (capIsBusy() != 0) {
                return;
            }
            /* fallthrough */
        case FACTORY_POWER_SCENE_APPLY_CHOICE:
            // Publish power before starting the accepted command's follow-up slot.
            if (capGetVariantKey() == FACTORY_POWER_CAP_ACCEPTED) {
                gameFlagSetNibble(GAME_FLAG_FACTORY_POWER_ON, 1);
                gameFlagSetNibble(GAME_FLAG_FACTORY_LAMP_PROGRESS, 1);
                if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
                    dryfieldFactorySetView11SpriteVisible(true);
                    dryfieldFactorySetView9SpriteVisible(1);
                } else {
                    dryfieldNightFactorySetView11SpriteVisible(true);
                    dryfieldNightFactorySetView9SpriteVisible(1);
                }
                capStartSequenceSlot(task->spawnArg1.value, CAP_PLAYBACK_DISPLAY_TRANSITION, FACTORY_POWER_FOLLOWUP_VARIANT_KEY);
            }
            task->state++;
            return;
        case FACTORY_POWER_SCENE_WAIT_FOLLOWUP:
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
