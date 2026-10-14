/* Part of the room variants library; see room_variants.h. */

/// Resolves the parking-lot destination from the night balcony scene's progress.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE`; `task` and `messageId` are unused.
/// Borrows a complete request and writable reply, which may alias. Copies the
/// request; execution selects the balcony-scene flag plus one for parking lot,
/// and queries retain the copied room. Returns 1 for every destination.
static s32 _roomVariantParkingLotMsg(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    *reply = *request;
    if (request->areaId == GAME_AREA_DRYFIELD_PARKING_LOT && request->queryOnly == ROOM_EVENT_EXECUTE) {
        reply->room = gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SCENE_SEEN) + 1;
    }
    return ROOM_VARIANT_TRANSITION_DIRECT;
}
