/* Part of the room variants library; see room_variants.h. */

/// Resolves the night motel rooms' destination on main street from story progress.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE`; `task` and `messageId` are unused.
/// Borrows a complete request and writable reply, which may alias. Copies the
/// request; execution selects room 3 at the final Dryfield chapter, otherwise
/// the balcony-scene flag plus one. Queries retain the copied room. Returns 1.
static s32 _roomVariantMainStreetMsg(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    s32 destinationRoom;
    s32 storyChapter;

    *reply = *request;
    if (request->areaId == GAME_AREA_DRYFIELD_NIGHT_MAIN_STREET && request->queryOnly == ROOM_EVENT_EXECUTE) {
        storyChapter = gameFlagGetNibble(GAME_FLAG_STORY_CHAPTER);
        if (storyChapter >= ROOM_VARIANT_DRYFIELD_FINAL_CHAPTER) {
            destinationRoom = ROOM_VARIANT_MAIN_STREET_FINAL_ROOM;
        } else {
            destinationRoom = gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SCENE_SEEN) + 1;
        }
        reply->room = destinationRoom;
    }
    return ROOM_VARIANT_TRANSITION_DIRECT;
}
