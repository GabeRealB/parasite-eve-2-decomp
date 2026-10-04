/* Part of the factory lift library; see factory_lift.h. */

/// Cutscene driver for the factory room: silences both weapons, runs the cap
/// (cutscene) command in `Task::spawnArg1`, then waits for the cap to report
/// event key 3 before setting the two progress flags and starting the follow-up
/// cap slot. Any state past 4 restores the weapons and kills the task.
void factoryPowerScene(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            Gp_MsgAllyWeapon(0);
            Gp_RunCapCmd(task->spawnArg1.value, 0);
            goto advance;
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
            if (Gp_CapBusy() != 0) {
                return;
            }
            /* fallthrough */
        case 3:
            if (Gp_GetCapEventKey() == 3) {
                gameFlagSetNibble(GAME_FLAG_FACTORY_POWER_ON, 1);
                gameFlagSetNibble(GAME_FLAG_FACTORY_LAMP_PROGRESS, 1);
                if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
                    factoryDayShowView11Sprite(1);
                    factoryDayShowView9Sprite(1);
                } else {
                    factoryNightShowView11Sprite(1);
                    factoryNightShowView9Sprite(1);
                }
                Gp_StartCapSlot(task->spawnArg1.value, 1, 2);
            }
        advance:
            task->state++;
            return;
        case 4:
            if (Gp_CapBusy() != 0) {
                return;
            }
            /* fallthrough */
        default:
            Gp_MsgPlayerWeapon(1);
            Gp_MsgAllyWeapon(1);
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
            taskKill(task);
            break;
    }
}
