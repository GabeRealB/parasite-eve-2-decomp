/* Part of the room variants library; see room_variants.h. */

/// Resolves the saloon's parking-lot exit and gates its first door-opening event.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE`; `task` and `messageId` are unused.
/// Borrows a complete request and writable reply, which may alias. Copies the
/// request and resolves parking lot's room only on execution. The gate receives
/// the request, so a distinct reply's resolved room is not latched for the event.
/// Returns the gate's result (1 bypassed, 2 eligible), or 1 for other areas.
/// Queries suppress the gate's CAP, flag and task-start effects.
/// The gate resets its event-start indication even on queries.
static s32 _roomVariantSaloonMsg(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        ROOM_VARIANT_SALOON_CAP_OPEN_PARKING_LOT_DOOR = 2,
        ROOM_VARIANT_SALOON_NO_COLLECTION_REQUIRED    = 0,
    };
    RoomEventReq eventRequest;
    u16          destinationArea;

    *reply          = *request;
    destinationArea = request->areaId;
    if (destinationArea == GAME_AREA_DRYFIELD_PARKING_LOT) {
        if (request->queryOnly == ROOM_EVENT_EXECUTE) {
            reply->room = gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SCENE_SEEN) + 1;
        }
        if (request->areaId == destinationArea) {
            eventRequest.capCmd        = ROOM_VARIANT_SALOON_CAP_OPEN_PARKING_LOT_DOOR;
            eventRequest.missingCapCmd = ROOM_VARIANT_SALOON_CAP_OPEN_PARKING_LOT_DOOR;
            eventRequest.firstSnd      = sndScriptResolveStageId(SOUND_SALOON_G_R_DOOR_UNLOCK);
            eventRequest.secondSnd     = sndScriptResolveStageId(SOUND_SALOON_G_R_DOOR_OPEN);
            eventRequest.flagId        = GAME_FLAG_SALOON_PARKING_LOT_DOOR_UNLOCKED;
            eventRequest.collectedBit  = ROOM_VARIANT_SALOON_NO_COLLECTION_REQUIRED;
            return _roomEventGate(&eventRequest, request);
        }
    }
    return ROOM_VARIANT_TRANSITION_DIRECT;
}
