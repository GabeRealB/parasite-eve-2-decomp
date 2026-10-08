/* Part of the factory lift library; see factory_lift.h. */

s32 factoryCommand(Task* task, s32 messageId, s32 command, s32 unusedArgument)
{
    enum {
        FACTORY_COMMAND_CAP_SCENE     = 1,
        FACTORY_COMMAND_WHITEOUT      = 2,
        FACTORY_COMMAND_LAMP          = 3,
        FACTORY_COMMAND_POWER_ON      = 5,
        FACTORY_COMMAND_PANEL_SESSION = 6,
        FACTORY_COMMAND_HATCH_SCENE   = 12,
        FACTORY_SPAWN_POWER_SCENE     = 0,
        FACTORY_SPAWN_LAMP_SCENE      = 1,
        FACTORY_SPAWN_CAP_SCENE       = 2,
        FACTORY_SPAWN_WHITEOUT_SCENE  = 3,
        FACTORY_SPAWN_HATCH_SCENE     = 6
    };
    switch (command) {
        case FACTORY_COMMAND_CAP_SCENE:
            taskSpawnFromTable(gFactorySpawnTable, FACTORY_SPAWN_CAP_SCENE, command, 0);
            break;
        case FACTORY_COMMAND_WHITEOUT:
            taskSpawnFromTable(gFactorySpawnTable, FACTORY_SPAWN_WHITEOUT_SCENE, command, 0);
            break;
        case FACTORY_COMMAND_LAMP:
            taskSpawnFromTable(gFactorySpawnTable, FACTORY_SPAWN_LAMP_SCENE, command, 0);
            break;
        case FACTORY_COMMAND_POWER_ON:
            taskSpawnFromTable(gFactorySpawnTable, FACTORY_SPAWN_POWER_SCENE, command, 0);
            break;
        case FACTORY_COMMAND_PANEL_SESSION:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
            companionSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            companionSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
            taskSpawnFromTable(gFactoryPanelSessionDesc, 0, 0, 0);
            break;
        case FACTORY_COMMAND_HATCH_SCENE:
            if (gameFlagGetNibble(GAME_FLAG_FACTORY_LIFT_POSITION) == FACTORY_LIFT_POSITION_TURNED) {
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                companionSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                taskSpawnFromTable(gFactorySpawnTable, FACTORY_SPAWN_HATCH_SCENE, command, 0);
            }
            break;
    }
    return 0;
}
