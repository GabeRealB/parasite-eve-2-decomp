/* Included water-tower departure resolver; declared in water_tower.h. */

s32 roomVariantWaterTowerMsg(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        WATER_TOWER_KITCHEN_DOOR_CAP_COMMAND         = 10,
        WATER_TOWER_KITCHEN_DOOR_MISSING_CAP_COMMAND = 6,
        WATER_TOWER_KITCHEN_DOOR_COLLECTION_BIT      = 0x10,
        WATER_TOWER_KITCHEN_DOOR_ITEM_ID             = 0x110,
        WATER_TOWER_COMPANION_SCHEDULE_TO_CLEAR      = 7,
        WATER_TOWER_COMPANION_ABSENT                 = 0,
        WATER_TOWER_PROGRESS_EVENT_FINISHED          = 2,
    };
    RoomEventReq eventRequest;
    s32          transitionResult;

    *reply = *request;
    if (request->areaId == GAME_AREA_DRYFIELD_G_R_KITCHEN) {
        eventRequest.capCmd        = WATER_TOWER_KITCHEN_DOOR_CAP_COMMAND;
        eventRequest.missingCapCmd = WATER_TOWER_KITCHEN_DOOR_MISSING_CAP_COMMAND;
        eventRequest.firstSnd      = sndScriptResolveStageId(SOUND_WATER_TOWER_KITCHEN_DOOR_UNLOCK);
        eventRequest.secondSnd     = sndScriptResolveStageId(SOUND_WATER_TOWER_KITCHEN_DOOR_OPEN);
        eventRequest.flagId        = GAME_FLAG_KITCHEN_WATER_TOWER_DOOR_UNLOCKED;
        eventRequest.collectedBit  = WATER_TOWER_KITCHEN_DOOR_COLLECTION_BIT;
        transitionResult           = _roomEventGate(&eventRequest, request);
        if (transitionResult == ROOM_VARIANT_TRANSITION_REFUSED) {
            transitionResult = ROOM_VARIANT_TRANSITION_HANDLED;
        }
        // The gate clears this indication even for queries; only its start path identifies the item.
        if (ROOM_EVENT_ACTIVE != 0) {
            itemSetIdentified(WATER_TOWER_KITCHEN_DOOR_ITEM_ID, 1);
        }
        return transitionResult;
    }
    // Kitchen departures return above and preserve both mechanism and companion state.
    if (request->queryOnly == ROOM_EVENT_EXECUTE && gameFlagGetNibble(GAME_FLAG_WATER_TOWER_MECHANISM_STATE) == GAME_FLAG_WATER_TOWER_MECHANISM_TOWER_OPERATED) {
        gameFlagSetNibble(GAME_FLAG_WATER_TOWER_MECHANISM_STATE, GAME_FLAG_WATER_TOWER_MECHANISM_TOWER_RESTORED);
    }
    if (request->areaId == GAME_AREA_DRYFIELD_WATER_TANK) {
        if (request->queryOnly == ROOM_EVENT_EXECUTE && gameFlagGetNibble(GAME_FLAG_COMPANION_2_SCHEDULE) == WATER_TOWER_COMPANION_SCHEDULE_TO_CLEAR) {
            gameFlagSetNibble(GAME_FLAG_COMPANION_2_SCHEDULE, WATER_TOWER_COMPANION_ABSENT);
        }
        if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD_NIGHT) {
            return ROOM_VARIANT_TRANSITION_DIRECT;
        }
        if (gameFlagGetNibble(GAME_FLAG_WATER_TOWER_PROGRESS) != WATER_TOWER_PROGRESS_EVENT_FINISHED) {
            return ROOM_VARIANT_TRANSITION_REFUSED;
        }
    }
    return ROOM_VARIANT_TRANSITION_DIRECT;
}
