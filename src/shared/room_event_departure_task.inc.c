/* Part of the room events library; see room_events.h. */

void roomEventDepartureTask(Task* task)
{
    enum {
        ROOM_EVENT_DEPARTURE_STATE_TURN        = 0,
        ROOM_EVENT_DEPARTURE_STATE_WAIT_TURN   = 1,
        ROOM_EVENT_DEPARTURE_STATE_START_SOUND = 2,
        ROOM_EVENT_DEPARTURE_STATE_WAIT_SOUND  = 3,
        ROOM_EVENT_DEPARTURE_STATE_RELOAD      = 4,
    };
    ActorTransform turnRequest;
    Task*          playerTask;

    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    switch (task->state) {
        case ROOM_EVENT_DEPARTURE_STATE_TURN:
            // The turn message consumes only yaw, synchronously.
            turnRequest.rot.vy = ROOM_DEPARTURE.facing;
            if (turnRequest.rot.vy == ROOM_DEPARTURE_SKIP_FACING) {
                task->state = ROOM_EVENT_DEPARTURE_STATE_START_SOUND;
                break;
            }
            TASK_MESSAGE_DISPATCH_POINTER(playerTask, GAME_ACTOR_MESSAGE_TURN_TO_YAW, &turnRequest, 0);
            task->state++;
            break;
        case ROOM_EVENT_DEPARTURE_STATE_WAIT_TURN:
            if (taskMessageDispatch(playerTask, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
                task->state++;
            }
            break;
        case ROOM_EVENT_DEPARTURE_STATE_START_SOUND:
            if (ROOM_DEPARTURE.sndEvent == 0) {
                task->state = ROOM_EVENT_DEPARTURE_STATE_RELOAD;
                break;
            }
            sndEvtRequestScriptStart(ROOM_DEPARTURE.sndEvent, 0, 0);
            task->state++;
            break;
        case ROOM_EVENT_DEPARTURE_STATE_WAIT_SOUND:
            if (sndScriptHasActiveId(ROOM_DEPARTURE.sndEvent) == 0) {
                task->state++;
            }
            break;
        case ROOM_EVENT_DEPARTURE_STATE_RELOAD:
            sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
            gDisplayState.spriteVariant                                 = 1;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage = ROOM_DEPARTURE.stage;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area  = ROOM_DEPARTURE.area;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp  = ROOM_DEPARTURE.warp;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room  = ROOM_DEPARTURE.room;
            taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
            taskKill(task);
            break;
        default:
            break;
    }
}
