/* Included General Store departure resolver; each carrier declares its static instance. */

/// Selects the General Store's underpass destination from event and switch state.
///
/// Borrows a writable reply and changes only its room selector. Before the
/// event, switch 1 selects room 5 when on or 6 when off. Afterwards, flag 0x53
/// selects base room 1 (clear) or 2 (set), with 2 added while switch 1 is off.
/// Flag 0x53's story meaning remains unproven; no transition effects are started.
static inline void _roomVariantGeneralStoreSelectUnderpassRoom(RoomEventMsg* reply)
{
    enum {
        GENERAL_STORE_UNDERPASS_ROOM_AFTER_EVENT      = 1,
        GENERAL_STORE_UNDERPASS_ROOM_FLAG_053         = 2,
        GENERAL_STORE_UNDERPASS_SWITCH_OFF_OFFSET     = 2,
        GENERAL_STORE_UNDERPASS_ROOM_BEFORE_EVENT_ON  = 5,
        GENERAL_STORE_UNDERPASS_ROOM_BEFORE_EVENT_OFF = 6,
    };
    if (gameFlagGetNibble(GAME_FLAG_UNDERPASS_EVENT_SEEN) != 0) {
        if (gameFlagGetNibble(GAME_FLAG_053) == 0) {
            reply->room = GENERAL_STORE_UNDERPASS_ROOM_AFTER_EVENT;
        } else {
            reply->room = GENERAL_STORE_UNDERPASS_ROOM_FLAG_053;
        }
        if (gameFlagGetNibble(GAME_FLAG_UNDERPASS_SWITCH_1) == 0) {
            reply->room = reply->room + GENERAL_STORE_UNDERPASS_SWITCH_OFF_OFFSET;
        }
    } else if (gameFlagGetNibble(GAME_FLAG_UNDERPASS_SWITCH_1) == 0) {
        reply->room = GENERAL_STORE_UNDERPASS_ROOM_BEFORE_EVENT_OFF;
    } else {
        reply->room = GENERAL_STORE_UNDERPASS_ROOM_BEFORE_EVENT_ON;
    }
}

/// Resolves the General Store's gas-station and underpass departures.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE`. Borrows complete eight-byte request
/// and reply records, which may alias, and copies the request first. Execution
/// selects destination rooms from progress flags; queries keep the input room.
/// Gas-station requests use the room event gate. Underpass execution either
/// starts the transition prompt or plays the blocked-departure CAP command.
/// Returns 1 for a direct departure or 2 for room-managed handling. Task and
/// message ID are unused. A started transition retains the request's warp and
/// room, so an aliased request sees the reply's resolved room at that point.
/// Keep the room loaded for its deferred event and transition tasks.
static s32 _roomVariantGeneralStoreMsg(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        GENERAL_STORE_GAS_STATION_BASE_ROOM           = 2,
        GENERAL_STORE_GAS_STATION_FINAL_ROOM          = 4,
        GENERAL_STORE_GAS_STATION_CAP_COMMAND         = 13,
        GENERAL_STORE_UNDERPASS_BLOCKED_CAP_COMMAND   = 14,
        GENERAL_STORE_UNDERPASS_TRANSITION_TASK_INDEX = 1,
    };
    RoomEventReq eventRequest;
    u16          areaId;
    s32          destinationRoom;

    *reply = *request;
    areaId = request->areaId;
    if (areaId == GAME_AREA_DRYFIELD_GAS_STATION && request->queryOnly == ROOM_EVENT_EXECUTE) {
        if (gameFlagGetNibble(GAME_FLAG_NIGHT_GAS_STATION_PROGRESS) == 0) {
            reply->room = areaId;
        } else {
            if (gameFlagGetNibble(GAME_FLAG_STORY_CHAPTER) >= ROOM_VARIANT_DRYFIELD_FINAL_CHAPTER) {
                destinationRoom = GENERAL_STORE_GAS_STATION_FINAL_ROOM;
            } else {
                destinationRoom = gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SCENE_SEEN) + GENERAL_STORE_GAS_STATION_BASE_ROOM;
            }
            reply->room = destinationRoom;
        }
    }
    if (request->areaId == GAME_AREA_DRYFIELD_UNDERPASS && request->queryOnly == ROOM_EVENT_EXECUTE) {
        _roomVariantGeneralStoreSelectUnderpassRoom(reply);
    }
    if (request->areaId == GAME_AREA_DRYFIELD_GAS_STATION) {
        eventRequest.capCmd        = GENERAL_STORE_GAS_STATION_CAP_COMMAND;
        eventRequest.missingCapCmd = GENERAL_STORE_GAS_STATION_CAP_COMMAND;
        eventRequest.firstSnd      = sndScriptResolveStageId(SOUND_GENERAL_STORE_DOOR_UNLOCK);
        eventRequest.secondSnd     = sndScriptResolveStageId(SOUND_GENERAL_STORE_DOOR_OPEN);
        eventRequest.flagId        = GAME_FLAG_GENERAL_STORE_DOOR_UNLOCKED;
        eventRequest.collectedBit  = ROOM_EVENT_GATE_NO_COLLECTION_REQUIRED;
        return _roomEventGate(&eventRequest, request);
    }
    if (request->areaId != GAME_AREA_DRYFIELD_UNDERPASS) {
        return ROOM_VARIANT_TRANSITION_DIRECT;
    }
    if (request->queryOnly != ROOM_EVENT_EXECUTE) {
        return ROOM_VARIANT_TRANSITION_HANDLED;
    }
    if (gameFlagGetNibble(GAME_FLAG_GENERAL_STORE_UNDERPASS_BLOCKED) == 0) {
        // The singleton destination is latched after requesting the deferred task.
        taskSpawnFromTable(gStoreTaskDescs, GENERAL_STORE_UNDERPASS_TRANSITION_TASK_INDEX, 0, 0);
        gStoreWarp = request->warp;
        gStoreRoom = request->room;
    } else {
        capRunCommandWithTransition(GENERAL_STORE_UNDERPASS_BLOCKED_CAP_COMMAND);
    }
    return ROOM_VARIANT_TRANSITION_HANDLED;
}
