/* Part of the factory lift library; see factory_lift.h. */

s32 roomVariantFactoryMsg(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        FACTORY_DRIVEWAY_PROGRESS_OPEN        = 2,
        FACTORY_GARAGE_LAMP_READY             = 2,
        FACTORY_GARAGE_FINAL_CHAPTER          = 4,
        FACTORY_GARAGE_REFUSAL_CAP_SLOT       = 4,
        FACTORY_BREEZEWAY_REFUSAL_CAP_COMMAND = 13,
        FACTORY_DRIVEWAY_CAP_COMMAND          = 14,
        FACTORY_DRIVEWAY_DEFAULT_ROOM         = 1,
        FACTORY_GARAGE_EARLY_ROOM             = 1,
        FACTORY_GARAGE_LATE_ROOM              = 2,
        FACTORY_DEPARTURE_REFUSAL_MAP_MARK    = 2,
    };
    RoomEventReq eventRequest;
    u8           stageId;

    // Preserve the complete request before execution-only destination selection.
    *reply = *request;
    if (request->areaId == GAME_AREA_DRYFIELD_DRIVEWAY) {
        stageId = gGameSession->location.loc.stage;
        if (stageId == GAME_STAGE_DRYFIELD) {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                if (gameFlagGetNibble(GAME_FLAG_DRIVEWAY_PROGRESS) >= FACTORY_DRIVEWAY_PROGRESS_OPEN) {
                    reply->room = stageId;
                } else {
                    reply->room = FACTORY_DRIVEWAY_DEFAULT_ROOM;
                }
            }
        } else if (request->queryOnly == ROOM_EVENT_EXECUTE) {
            reply->room = gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SCENE_SEEN) + 1;
        }
    }
    if (request->areaId == GAME_AREA_DRYFIELD_GARAGE) {
        if (request->queryOnly == ROOM_EVENT_EXECUTE) {
            if (gameFlagGetNibble(GAME_FLAG_STORY_CHAPTER) < FACTORY_GARAGE_FINAL_CHAPTER) {
                reply->room = FACTORY_GARAGE_EARLY_ROOM;
            } else {
                reply->room = FACTORY_GARAGE_LATE_ROOM;
            }
        }
        if (request->areaId == GAME_AREA_DRYFIELD_GARAGE) {
            if (gameFlagGetNibble(GAME_FLAG_FACTORY_LAMP_PROGRESS) != FACTORY_GARAGE_LAMP_READY) {
                if (request->queryOnly != ROOM_EVENT_EXECUTE) {
                    return ROOM_VARIANT_TRANSITION_REFUSED;
                }
                capStartSequenceSlot(FACTORY_GARAGE_REFUSAL_CAP_SLOT, CAP_PLAYBACK_DISPLAY_TRANSITION, 0);
                gameFlagSetNibbleIfPresent(request->flagId, FACTORY_DEPARTURE_REFUSAL_MAP_MARK);
                return ROOM_VARIANT_TRANSITION_REFUSED;
            }
        }
    }
    if (request->areaId == GAME_AREA_DRYFIELD_BREEZEWAY) {
        if (gameFlagGetNibble(GAME_FLAG_BREEZEWAY_FACTORY_DOOR_UNLOCKED) == 0) {
            if (request->queryOnly != ROOM_EVENT_EXECUTE) {
                return ROOM_VARIANT_TRANSITION_REFUSED;
            }
            gameFlagSetNibbleIfPresent(request->flagId, FACTORY_DEPARTURE_REFUSAL_MAP_MARK);
            capRunCommandWithTransition(FACTORY_BREEZEWAY_REFUSAL_CAP_COMMAND);
            return ROOM_VARIANT_TRANSITION_REFUSED;
        }
    }
    if (request->areaId == GAME_AREA_DRYFIELD_DRIVEWAY) {
        eventRequest.capCmd        = FACTORY_DRIVEWAY_CAP_COMMAND;
        eventRequest.missingCapCmd = FACTORY_DRIVEWAY_CAP_COMMAND;
        eventRequest.firstSnd      = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_FACTORY, 0x13);
        eventRequest.secondSnd     = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_FACTORY, 3);
        eventRequest.flagId        = -GAME_FLAG_030;
        eventRequest.collectedBit  = ROOM_EVENT_GATE_NO_COLLECTION_REQUIRED;
        return _roomEventGate(&eventRequest, request);
    }
    return ROOM_VARIANT_TRANSITION_DIRECT;
}
