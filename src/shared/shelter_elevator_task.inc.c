/* Part of the shelter elevator library; see shelter_elevator.h. */

void shelterElevatorTask(Task* task)
{
    enum {
        SHELTER_ELEVATOR_B1_ARRIVAL     = 3,
        SHELTER_ELEVATOR_B2_ARRIVAL     = 2,
        SHELTER_ELEVATOR_B3_ARRIVAL     = 3,
        SHELTER_ELEVATOR_DEFAULT_ROOM   = 1,
        SHELTER_ELEVATOR_SPRITE_VARIANT = 1,
        SHELTER_ELEVATOR_CHOICE_B1      = 11,
        SHELTER_ELEVATOR_CHOICE_B2      = 12,
        SHELTER_ELEVATOR_CHOICE_B3      = 13,
    };
    enum {
        SHELTER_ELEVATOR_STATE_HOLD       = 0,
        SHELTER_ELEVATOR_STATE_WAIT_MENU  = 1,
        SHELTER_ELEVATOR_STATE_SELECT     = 2,
        SHELTER_ELEVATOR_STATE_WAIT_SOUND = 3,
        SHELTER_ELEVATOR_STATE_RELOAD     = 4,
    };
    RoomEventMsg destination;
    RoomEventMsg resolvedDestination;

    switch (task->state) {
        case SHELTER_ELEVATOR_STATE_HOLD:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            task->state++;
            break;
        case SHELTER_ELEVATOR_STATE_WAIT_MENU:
            if (capIsBusy() == 0) {
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
                task->state++;
                break;
            }
            break;
        case SHELTER_ELEVATOR_STATE_SELECT:
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            switch (capGetVariantKey()) {
                case SHELTER_ELEVATOR_CHOICE_B1:
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = GAME_AREA_SHELTER_B1_ELEVATOR_HALL;
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = SHELTER_ELEVATOR_B1_ARRIVAL;
                    break;
                case SHELTER_ELEVATOR_CHOICE_B2:
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = GAME_AREA_SHELTER_B2_ELEVATOR_HALL;
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = SHELTER_ELEVATOR_B2_ARRIVAL;
                    break;
                case SHELTER_ELEVATOR_CHOICE_B3:
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = GAME_AREA_SHELTER_B3_ELEVATOR_HALL;
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = SHELTER_ELEVATOR_B3_ARRIVAL;
                    break;
                default:
                    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                    gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
                    taskKill(task);
                    break;
            }
            task->state++;
            break;
        case SHELTER_ELEVATOR_STATE_WAIT_SOUND:
            if (sndScriptHasActiveId(task->spawnArg1.value) == 0) {
                task->state++;
            }
            break;
        case SHELTER_ELEVATOR_STATE_RELOAD:
            sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
            // Only the resolver inputs are initialized; it changes the reply room.
            destination.room      = SHELTER_ELEVATOR_DEFAULT_ROOM;
            destination.queryOnly = ROOM_EVENT_EXECUTE;
            destination.areaId    = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area;
            destination.warp      = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp;
            resolvedDestination   = destination;
            mapShelterRoomVariantResolve(&destination, &resolvedDestination);
            gDisplayState.spriteVariant                                = SHELTER_ELEVATOR_SPRITE_VARIANT;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = resolvedDestination.warp;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = resolvedDestination.room;
            taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
            taskKill(task);
            break;
    }
}
