/* Part of the room variants library; see room_variants.h. */

/// Retires the two individual motel keys and identifies the Bronco masterkey.
static inline void _roomVariantExchangeMotelKeys(void)
{
    inventoryClearCollectedBit(INVENTORY_COLLECTION_ID_MOTEL_ROOM_6_KEY);
    inventoryClearCollectedBit(INVENTORY_COLLECTION_ID_LOBBY_KEY);
    itemSetIdentified(INVENTORY_COLLECTION_ID_BRONCO_MASTERKEY, 1);
}

/// Selects the carrier-declared s32 (Task*, s32, const RoomEventMsg*, RoomEventMsg*) handler.
///
/// The day balcony uses the private default. The night balcony binds its
/// overlay-private `roomVariantMotelBalconyDoorsMsg` around this fragment.
/// The replacement is one function identifier, without argument evaluation.
#ifndef ROOM_VARIANT_MOTEL_BALCONY_DOORS_MSG
#define ROOM_VARIANT_MOTEL_BALCONY_DOORS_MSG _roomVariantMotelBalconyDoorsMsg

/// Resolves balcony departures and starts the motel doors' unlock events.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE`; `task` and `messageId` are unused.
/// Borrows a complete request and writable reply, which may alias. Copies the
/// request and resolves room 5, parking lot and loft only on execution.
/// Unlocking room 5 and loft requires the Bronco masterkey; room 6 uses its own key.
/// A started masterkey event retires the individual keys; a started room-6
/// event arms its scene and objective. Queries leave those effects untouched.
/// The gate resets its event-start indication even on queries.
/// Returns 1 for ordinary departure and 2 for an eligible or refused door event.
#endif
s32 ROOM_VARIANT_MOTEL_BALCONY_DOORS_MSG(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        ROOM_VARIANT_MOTEL_DOOR_COLLECTION_MASK   = 0x7F,
        ROOM_VARIANT_MOTEL_LOFT_BEFORE_EVENT_ROOM = 1,
        ROOM_VARIANT_MOTEL_LOFT_AFTER_EVENT_ROOM  = 2,
        ROOM_VARIANT_MOTEL_CAP_UNLOCK_ROOM_5      = 7,
        ROOM_VARIANT_MOTEL_CAP_ROOM_5_KEY_MISSING = 4,
        ROOM_VARIANT_MOTEL_CAP_UNLOCK_LOFT        = 5,
        ROOM_VARIANT_MOTEL_CAP_LOFT_KEY_MISSING   = 2,
        ROOM_VARIANT_MOTEL_CAP_UNLOCK_ROOM_6      = 6,
        ROOM_VARIANT_MOTEL_CAP_ROOM_6_KEY_MISSING = 3,
        ROOM_VARIANT_MOTEL_ROOM_6_SCENE_EVENT     = 3,
        ROOM_VARIANT_MOTEL_ROOM_6_OBJECTIVE       = 0xC,
    };
    RoomEventReq eventRequest;
    s32          loftEventPending;
    s32          transitionResult;

    *reply = *request;
    if (request->areaId == GAME_AREA_DRYFIELD_MOTEL_ROOM_5 && request->queryOnly == ROOM_EVENT_EXECUTE) {
        reply->room = gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SCENE_SEEN) + 1;
    }
    if (request->areaId == GAME_AREA_DRYFIELD_PARKING_LOT && request->queryOnly == ROOM_EVENT_EXECUTE) {
        reply->room = gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SCENE_SEEN) + 1;
    }
    if (request->areaId == GAME_AREA_DRYFIELD_MOTEL_LOFT && request->queryOnly == ROOM_EVENT_EXECUTE) {
        loftEventPending = gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_LOFT_EVENT_SEEN) == 0;
        reply->room      = loftEventPending ? ROOM_VARIANT_MOTEL_LOFT_BEFORE_EVENT_ROOM : ROOM_VARIANT_MOTEL_LOFT_AFTER_EVENT_ROOM;
    }
    // Gate the resolved reply so the deferred event keeps its destination room.
    if (request->areaId == GAME_AREA_DRYFIELD_MOTEL_ROOM_5) {
        eventRequest.capCmd        = ROOM_VARIANT_MOTEL_CAP_UNLOCK_ROOM_5;
        eventRequest.missingCapCmd = ROOM_VARIANT_MOTEL_CAP_ROOM_5_KEY_MISSING;
        eventRequest.firstSnd      = sndScriptResolveStageId(SOUND_MOTEL_BALCONY_DOOR_UNLOCK);
        eventRequest.secondSnd     = sndScriptResolveStageId(SOUND_MOTEL_BALCONY_DOOR_OPEN);
        eventRequest.flagId        = GAME_FLAG_MOTEL_ROOM_5_DOOR_UNLOCKED;
        eventRequest.collectedBit  = INVENTORY_COLLECTION_ID_BRONCO_MASTERKEY & ROOM_VARIANT_MOTEL_DOOR_COLLECTION_MASK;
        transitionResult           = _roomEventGate(&eventRequest, reply);
        if (ROOM_EVENT_ACTIVE != 0) {
            _roomVariantExchangeMotelKeys();
        }
    } else if (request->areaId == GAME_AREA_DRYFIELD_MOTEL_LOFT) {
        eventRequest.capCmd        = ROOM_VARIANT_MOTEL_CAP_UNLOCK_LOFT;
        eventRequest.missingCapCmd = ROOM_VARIANT_MOTEL_CAP_LOFT_KEY_MISSING;
        eventRequest.firstSnd      = sndScriptResolveStageId(SOUND_MOTEL_BALCONY_DOOR_UNLOCK);
        eventRequest.secondSnd     = sndScriptResolveStageId(SOUND_MOTEL_BALCONY_DOOR_OPEN);
        eventRequest.flagId        = GAME_FLAG_MOTEL_LOFT_DOOR_UNLOCKED;
        eventRequest.collectedBit  = INVENTORY_COLLECTION_ID_BRONCO_MASTERKEY & ROOM_VARIANT_MOTEL_DOOR_COLLECTION_MASK;
        transitionResult           = _roomEventGate(&eventRequest, reply);
        if (ROOM_EVENT_ACTIVE != 0) {
            _roomVariantExchangeMotelKeys();
        }
    } else if (request->areaId == GAME_AREA_DRYFIELD_MOTEL_ROOM_6) {
        eventRequest.capCmd        = ROOM_VARIANT_MOTEL_CAP_UNLOCK_ROOM_6;
        eventRequest.missingCapCmd = ROOM_VARIANT_MOTEL_CAP_ROOM_6_KEY_MISSING;
        eventRequest.firstSnd      = sndScriptResolveStageId(SOUND_MOTEL_BALCONY_DOOR_UNLOCK);
        eventRequest.secondSnd     = sndScriptResolveStageId(SOUND_MOTEL_BALCONY_DOOR_OPEN);
        eventRequest.flagId        = GAME_FLAG_MOTEL_ROOM_6_DOOR_UNLOCKED;
        eventRequest.collectedBit  = INVENTORY_COLLECTION_ID_MOTEL_ROOM_6_KEY & ROOM_VARIANT_MOTEL_DOOR_COLLECTION_MASK;
        transitionResult           = _roomEventGate(&eventRequest, reply);
        if (ROOM_EVENT_ACTIVE != 0) {
            gameFlagSetNibble(GAME_FLAG_030, 1);
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = ROOM_VARIANT_MOTEL_ROOM_6_SCENE_EVENT;
            gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, ROOM_VARIANT_MOTEL_ROOM_6_OBJECTIVE);
        }
    } else {
        return ROOM_VARIANT_TRANSITION_DIRECT;
    }
    // A missing-key caption consumes this departure rather than the normal refusal path.
    if (transitionResult == ROOM_VARIANT_TRANSITION_REFUSED) {
        transitionResult = ROOM_VARIANT_TRANSITION_HANDLED;
    }
    return transitionResult;
}
#undef ROOM_VARIANT_MOTEL_BALCONY_DOORS_MSG
