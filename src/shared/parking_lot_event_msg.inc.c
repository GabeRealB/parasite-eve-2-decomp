/* Part of the parking lot library; see parking_lot.h. */

s32 parkingLotResolveMessage(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        PARKING_LOT_CAP_UNLOCK_LOBBY       = 6,
        PARKING_LOT_CAP_LOBBY_KEY_MISSING  = 1,
        PARKING_LOT_CAP_UNLOCK_SALOON      = 3,
        PARKING_LOT_CAP_SALOON_KEY_MISSING = 2,
        PARKING_LOT_SALOON_ITEM            = 0x110,
        PARKING_LOT_COLLECTION_MASK        = 0x7F,
        PARKING_LOT_TRANSITION_KEY_MISSING = 0,
        PARKING_LOT_TRANSITION_ORDINARY    = 1,
        PARKING_LOT_TRANSITION_HANDLED     = 2,
    };
    RoomEventReq eventRequest;
    s32          transitionResult;
    s32          roomVariant;
    s32          storyChapter;
    s32          balconyRoom;

    // The gate must latch the resolved destination, including an aliased reply.
    *reply = *request;
    if ((request->areaId == GAME_AREA_DRYFIELD_MAIN_STREET) && (request->queryOnly == ROOM_EVENT_EXECUTE)) {
        storyChapter = gameFlagGetNibble(GAME_FLAG_STORY_CHAPTER);
        if (storyChapter >= 4) {
            roomVariant = 3;
        } else {
            roomVariant = gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SCENE_SEEN) + 1;
        }
        reply->room = roomVariant;
    }
    if ((request->areaId == GAME_AREA_DRYFIELD_MOTEL_BALCONY) && (request->queryOnly == ROOM_EVENT_EXECUTE)) {
        balconyRoom = gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SCENE_SEEN);
        if (balconyRoom == 0) {
            balconyRoom = 1;
        } else {
            balconyRoom = 3;
        }
        reply->room = balconyRoom;
    }
    if (request->areaId == GAME_AREA_DRYFIELD_MOTEL_LOBBY) {
        eventRequest.capCmd        = PARKING_LOT_CAP_UNLOCK_LOBBY;
        eventRequest.missingCapCmd = PARKING_LOT_CAP_LOBBY_KEY_MISSING;
        eventRequest.firstSnd      = sndScriptResolveStageId(SOUND_PARKING_LOT_DOOR_UNLOCK);
        eventRequest.secondSnd     = sndScriptResolveStageId(SOUND_PARKING_LOT_DOOR_OPEN);
        eventRequest.flagId        = GAME_FLAG_PARKING_LOT_LOBBY_DOOR_UNLOCKED;
        eventRequest.collectedBit  = INVENTORY_COLLECTION_ID_LOBBY_KEY & PARKING_LOT_COLLECTION_MASK;
        transitionResult           = _roomEventGate(&eventRequest, reply);
        if (transitionResult == PARKING_LOT_TRANSITION_KEY_MISSING) {
            transitionResult = PARKING_LOT_TRANSITION_HANDLED;
        }
        if (ROOM_EVENT_ACTIVE != 0) {
            areaApplySavedUpdates(gParkingLotAreaRecs);
            gameFlagSetNibble(GAME_FLAG_046, 1);
            gameFlagSetNibble(GAME_FLAG_097, 1);
        }
    } else if (request->areaId == GAME_AREA_DRYFIELD_SALOON_G_R) {
        eventRequest.capCmd        = PARKING_LOT_CAP_UNLOCK_SALOON;
        eventRequest.missingCapCmd = PARKING_LOT_CAP_SALOON_KEY_MISSING;
        eventRequest.firstSnd      = sndScriptResolveStageId(SOUND_PARKING_LOT_DOOR_UNLOCK);
        eventRequest.secondSnd     = sndScriptResolveStageId(SOUND_PARKING_LOT_DOOR_OPEN);
        eventRequest.flagId        = GAME_FLAG_SALOON_PARKING_LOT_DOOR_UNLOCKED;
        eventRequest.collectedBit  = PARKING_LOT_SALOON_ITEM & PARKING_LOT_COLLECTION_MASK;
        transitionResult           = _roomEventGate(&eventRequest, reply);
        if (ROOM_EVENT_ACTIVE != 0) {
            itemSetIdentified(PARKING_LOT_SALOON_ITEM, true);
        }
    } else {
        return PARKING_LOT_TRANSITION_ORDINARY;
    }
    return transitionResult;
}
