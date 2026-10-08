/* Part of the room events library; see room_events.h. */

/// Requests the staged event's 30-frame blackout through session reload.
///
/// Initializes a writable room-owned fade and lends it to `fadeScreenTask`.
/// Keep the record loaded and unchanged until reload discards the session task
/// list. Uses the default foreground ordering tag; starts no return ramp and
/// does not wait for full coverage. Spawn failure leaves the record initialized
/// and the event continues toward reload without a blackout.
static inline void _roomEventStartStagedBlackout(ScreenFade* fade)
{
    enum {
        ROOM_EVENT_STAGED_FADE_FRAMES         = 30,
        ROOM_EVENT_STAGED_FADE_BANK           = 1,
        ROOM_EVENT_STAGED_FADE_SLOT           = 0x31,
        ROOM_EVENT_STAGED_FADE_DEFAULT_OT_TAG = 0,
    };
    fade->blend      = SCREEN_FADE_SUBTRACT;
    fade->phase      = SCREEN_FADE_RUNNING;
    fade->rampFrames = ROOM_EVENT_STAGED_FADE_FRAMES;
    taskSpawn(ROOM_EVENT_STAGED_FADE_BANK, ROOM_EVENT_STAGED_FADE_SLOT, ROOM_EVENT_STAGED_FADE_DEFAULT_OT_TAG, fade);
}

void roomEventStagedTask(Task* task)
{
    enum {
        ROOM_EVENT_STAGED_RESTORE_ACTORS_ON_CAP_EXIT = 1,
        ROOM_EVENT_STAGED_STATE_START                = 0,
        ROOM_EVENT_STAGED_STATE_WAIT_CAP             = 1,
        ROOM_EVENT_STAGED_STATE_START_SOUND          = 2,
        ROOM_EVENT_STAGED_STATE_WAIT_SOUND           = 3,
        ROOM_EVENT_STAGED_STATE_RELOAD               = 4,
    };
    switch (task->state) {
        case ROOM_EVENT_STAGED_STATE_START:
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            capRunCommand(ROOM_EVENT_LATCHED.capCmd, CAP_PLAYBACK_IN_PLACE);
            D_80115690 = ROOM_EVENT_STAGED_RESTORE_ACTORS_ON_CAP_EXIT;
            task->state++;
            break;
        case ROOM_EVENT_STAGED_STATE_WAIT_CAP:
            if (capIsBusy() == 0) {
                if (ROOM_EVENT_LATCHED.fade != 0) {
                    // Hold the blackout until reload discards the old task list.
                    _roomEventStartStagedBlackout(&ROOM_EVENT_FADE);
                }
                task->state++;
            }
            break;
        case ROOM_EVENT_STAGED_STATE_START_SOUND:
            if (ROOM_EVENT_LATCHED.stageSnd != 0) {
                sndEvtRequestStageScriptStart(ROOM_EVENT_LATCHED.stageSnd, 0, 0);
                task->state++;
            } else {
                task->state = ROOM_EVENT_STAGED_STATE_RELOAD;
            }
            break;
        case ROOM_EVENT_STAGED_STATE_WAIT_SOUND:
            if (sndScriptHasActiveId(sndScriptResolveStageId(ROOM_EVENT_LATCHED.stageSnd)) == 0) {
                task->state++;
            }
            break;
        case ROOM_EVENT_STAGED_STATE_RELOAD:
            sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
            gDisplayState.spriteVariant                                = 1;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = gRoomEventStagedMsg.areaId;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = gRoomEventStagedMsg.warp;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = gRoomEventStagedMsg.room;
            taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
            taskKill(task);
            break;
    }
}
