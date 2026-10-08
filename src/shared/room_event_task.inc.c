/* Part of the room events library; see room_events.h. */

void roomEventTask(Task* task)
{
    enum {
        ROOM_EVENT_STATE_START              = 0,
        ROOM_EVENT_STATE_WAIT_FIRST_SOUND   = 1,
        ROOM_EVENT_STATE_GAP                = 2,
        ROOM_EVENT_STATE_START_SECOND_SOUND = 3,
        ROOM_EVENT_STATE_WAIT_SECOND_SOUND  = 4,
        ROOM_EVENT_STATE_RELOAD             = 5,
    };
    switch (task->state) {
        case ROOM_EVENT_STATE_START:
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            capRunCommandWithTransition(ROOM_EVENT_REQ.capCmd);
            if (ROOM_EVENT_REQ.firstSnd != 0) {
                sndEvtRequestScriptStart(ROOM_EVENT_REQ.firstSnd, 0, 0);
                task->state++;
            } else {
                task->state = ROOM_EVENT_STATE_GAP;
            }
            break;
        case ROOM_EVENT_STATE_WAIT_FIRST_SOUND:
            if (sndScriptHasActiveId(ROOM_EVENT_REQ.firstSnd) == 0) {
                task->state++;
            }
            break;
        case ROOM_EVENT_STATE_GAP:
            task->state++;
            break;
        case ROOM_EVENT_STATE_START_SECOND_SOUND:
            if (ROOM_EVENT_REQ.secondSnd != 0) {
                sndEvtRequestScriptStart(ROOM_EVENT_REQ.secondSnd, 0, 0);
                task->state++;
            } else {
                task->state = ROOM_EVENT_STATE_RELOAD;
            }
            break;
        case ROOM_EVENT_STATE_WAIT_SECOND_SOUND:
            if (sndScriptHasActiveId(ROOM_EVENT_REQ.secondSnd) == 0) {
                task->state++;
            }
            break;
        // Leave control restoration to the session reload and room startup.
        case ROOM_EVENT_STATE_RELOAD:
            sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
            gDisplayState.spriteVariant                                = 1;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = gRoomEventMsg.areaId;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = gRoomEventMsg.warp;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = (u8)gRoomEventMsg.room;
            taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
            taskKill(task);
            break;
    }
}
