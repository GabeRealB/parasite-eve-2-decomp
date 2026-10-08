/* Part of the Dryfield main street library; see main_street.h. */

/// Retires the individual motel keys and identifies the masterkey after a door start.
static inline void _mainStreetExchangeMotelKeys(void)
{
    inventoryClearCollectedBit(INVENTORY_COLLECTION_ID_MOTEL_ROOM_6_KEY);
    inventoryClearCollectedBit(INVENTORY_COLLECTION_ID_LOBBY_KEY);
    itemSetIdentified(INVENTORY_COLLECTION_ID_BRONCO_MASTERKEY, true);
}

/// Resolves main-street departures and starts staged scenes or motel door events.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE`; receiver and message ID are unused.
/// Borrows a complete request and writable reply, which may alias. Copies the
/// request, then resolves destination variants on execution. Eligible motel
/// room-1/2 scenes latch the reply and event before requesting a task; room-3/4
/// doors use the key gate and retire individual motel keys on a start request.
/// Query mode suppresses those effects but still clears the start indication
/// in the selected gate. Returns 1 for ordinary departure and 2 for an eligible
/// scene or a handled refusal. Keep latched room records unchanged and loaded
/// until their event task requests the session reload; spawn failure does not
/// roll back the flags or the start indication.
static s32 _mainStreetResolveMessage(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        MAIN_STREET_CAP_DRIVEWAY_BLOCKED    = 19,
        MAIN_STREET_CAP_MOTEL_ROOM_1_SCENE  = 3,
        MAIN_STREET_CAP_MOTEL_ROOM_2_SCENE  = 4,
        MAIN_STREET_CAP_UNLOCK_MOTEL_ROOM_3 = 10,
        MAIN_STREET_CAP_UNLOCK_MOTEL_ROOM_4 = 11,
        MAIN_STREET_CAP_ROOM_3_KEY_MISSING  = 5,
        MAIN_STREET_CAP_ROOM_4_KEY_MISSING  = 6,
        MAIN_STREET_COLLECTION_MASK         = 0x7F,
        MAIN_STREET_TRANSITION_KEY_MISSING  = 0,
        MAIN_STREET_TRANSITION_ORDINARY     = 1,
        MAIN_STREET_TRANSITION_HANDLED      = 2,
    };
    RoomEventReq     eventRequest;
    RoomLatchedEvent stagedEvent;
    s32              transitionResult;

/// Checks and commits one staged-scene request, returning its transition reply.
///
/// Used only in this resolver. Captures stagedEvent and reply plus the room's
/// event snapshots, descriptor and start indication. Evaluates the flag lookup
/// before the zero sentinel; queries clear only the indication. Returns from
/// the containing handler (2 eligible, 1 already latched); spawn failure still
/// commits the flag and indication. Expands to one compound statement.
#define MAIN_STREET_LATCH_STAGED_SCENE()                                             \
    {                                                                                \
        gMainStreetEventSpawned.eventStarted = 0;                                    \
        if (gameFlagGetNibble(stagedEvent.flagId) == 0 || stagedEvent.flagId == 0) { \
            if (reply->queryOnly == ROOM_EVENT_EXECUTE) {                            \
                gRoomEventStagedMsg = *reply;                                        \
                ROOM_EVENT_LATCHED  = stagedEvent;                                   \
                if (stagedEvent.flagId != 0) {                                       \
                    gameFlagSetNibble(stagedEvent.flagId, 1);                        \
                }                                                                    \
                taskSpawnFromTable(&gMainStreetEventTaskDesc, 0, 0, 0);              \
                gMainStreetEventSpawned.eventStarted = 1;                            \
                return MAIN_STREET_TRANSITION_HANDLED;                               \
            }                                                                        \
            return MAIN_STREET_TRANSITION_HANDLED;                                   \
        }                                                                            \
        return MAIN_STREET_TRANSITION_ORDINARY;                                      \
    }

    // Resolve ordinary departures before a gate latches the destination.
    *reply = *request;
    if (request->areaId == GAME_AREA_DRYFIELD_DRIVEWAY) {
        if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                if (gameFlagGetNibble(GAME_FLAG_DRIVEWAY_PROGRESS) >= 2) {
                    reply->room = 2;
                } else {
                    reply->room = 1;
                }
            }
        } else if (request->queryOnly == ROOM_EVENT_EXECUTE) {
            reply->room = gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SCENE_SEEN) + 1;
        }
    }
    if (request->areaId == GAME_AREA_DRYFIELD_GAS_STATION && request->queryOnly == ROOM_EVENT_EXECUTE) {
        if (gameFlagGetNibble(GAME_FLAG_NIGHT_GAS_STATION_PROGRESS) == 0) {
            reply->room = 1;
        } else if (gameFlagGetNibble(GAME_FLAG_STORY_CHAPTER) >= 4) {
            reply->room = 4;
        } else {
            reply->room = gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SCENE_SEEN) + 2;
        }
    }
    if (request->areaId == GAME_AREA_DRYFIELD_PARKING_LOT && request->queryOnly == ROOM_EVENT_EXECUTE) {
        reply->room = gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SCENE_SEEN) + 1;
    }
    if (request->areaId == GAME_AREA_DRYFIELD_DRIVEWAY && gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SCENE_SEEN) != 0) {
        if (request->queryOnly == ROOM_EVENT_EXECUTE) {
            gameFlagSetNibbleIfPresent(request->flagId, 2);
            capRunCommandWithTransition(MAIN_STREET_CAP_DRIVEWAY_BLOCKED);
            return MAIN_STREET_TRANSITION_HANDLED;
        }
        return MAIN_STREET_TRANSITION_HANDLED;
    }
    // Commit scene snapshots and flags before the unchecked spawn request.
    if (request->areaId == GAME_AREA_DRYFIELD_MOTEL_ROOM_1) {
        stagedEvent.capCmd   = MAIN_STREET_CAP_MOTEL_ROOM_1_SCENE;
        stagedEvent.stageSnd = SOUND_MAIN_STREET_MOTEL_DOOR_OPEN;
        stagedEvent.flagId   = GAME_FLAG_MAIN_STREET_TO_MOTEL_ROOM_1_SCENE;
        stagedEvent.fade     = 0;
        MAIN_STREET_LATCH_STAGED_SCENE();
    } else if (request->areaId == GAME_AREA_DRYFIELD_MOTEL_ROOM_2) {
        stagedEvent.capCmd   = MAIN_STREET_CAP_MOTEL_ROOM_2_SCENE;
        stagedEvent.stageSnd = SOUND_MAIN_STREET_MOTEL_DOOR_OPEN;
        stagedEvent.flagId   = GAME_FLAG_MAIN_STREET_TO_MOTEL_ROOM_2_SCENE;
        stagedEvent.fade     = 0;
        MAIN_STREET_LATCH_STAGED_SCENE();
    } else if (request->areaId == GAME_AREA_DRYFIELD_MOTEL_ROOM_3) {
        eventRequest.capCmd        = MAIN_STREET_CAP_UNLOCK_MOTEL_ROOM_3;
        eventRequest.missingCapCmd = MAIN_STREET_CAP_ROOM_3_KEY_MISSING;
        eventRequest.firstSnd      = sndScriptResolveStageId(SOUND_MAIN_STREET_MOTEL_DOOR_UNLOCK);
        eventRequest.secondSnd     = sndScriptResolveStageId(SOUND_MAIN_STREET_MOTEL_DOOR_OPEN);
        eventRequest.flagId        = GAME_FLAG_MOTEL_ROOM_3_DOOR_UNLOCKED;
        eventRequest.collectedBit  = INVENTORY_COLLECTION_ID_BRONCO_MASTERKEY & MAIN_STREET_COLLECTION_MASK;
        transitionResult           = _roomEventGate(&eventRequest, reply);
        if (transitionResult == MAIN_STREET_TRANSITION_KEY_MISSING) {
            transitionResult = MAIN_STREET_TRANSITION_HANDLED;
        }
        if (ROOM_EVENT_ACTIVE != 0) {
            _mainStreetExchangeMotelKeys();
        }
        if (request->queryOnly == ROOM_EVENT_EXECUTE && gameFlagGetNibble(GAME_FLAG_093) == 0) {
            gameFlagSetNibbleIfPresent(request->flagId, 0);
        }
        return transitionResult;
    } else if (request->areaId == GAME_AREA_DRYFIELD_MOTEL_ROOM_4) {
        eventRequest.capCmd        = MAIN_STREET_CAP_UNLOCK_MOTEL_ROOM_4;
        eventRequest.missingCapCmd = MAIN_STREET_CAP_ROOM_4_KEY_MISSING;
        eventRequest.firstSnd      = sndScriptResolveStageId(SOUND_MAIN_STREET_MOTEL_DOOR_UNLOCK);
        eventRequest.secondSnd     = sndScriptResolveStageId(SOUND_MAIN_STREET_MOTEL_DOOR_OPEN);
        eventRequest.flagId        = GAME_FLAG_MOTEL_ROOM_4_DOOR_UNLOCKED;
        eventRequest.collectedBit  = INVENTORY_COLLECTION_ID_BRONCO_MASTERKEY & MAIN_STREET_COLLECTION_MASK;
        transitionResult           = _roomEventGate(&eventRequest, reply);
        if (transitionResult == MAIN_STREET_TRANSITION_KEY_MISSING) {
            transitionResult = MAIN_STREET_TRANSITION_HANDLED;
        }
        if (ROOM_EVENT_ACTIVE != 0) {
            _mainStreetExchangeMotelKeys();
        }
        if (request->queryOnly == ROOM_EVENT_EXECUTE && gameFlagGetNibble(GAME_FLAG_094) == 0) {
            gameFlagSetNibbleIfPresent(request->flagId, 0);
        }
        return transitionResult;
    }
    return MAIN_STREET_TRANSITION_ORDINARY;
}

#undef MAIN_STREET_LATCH_STAGED_SCENE
