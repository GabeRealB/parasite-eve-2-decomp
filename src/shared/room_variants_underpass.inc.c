/* Part of the room variants library; see room_variants.h. */

/// Resolves the underpass's water-hole and cellar destinations from its switches.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE`; `task` and `messageId` are unused.
/// Borrows a complete request and writable reply, which may alias. Copies the
/// request; execution chooses room 1 for a set switch or 2 for a clear switch.
/// Flag 0x53 adds two to the water-hole selector; its story meaning is unproven.
/// Queries retain the copied room. Always returns 1.
static s32 _roomVariantUnderpassMsg(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        ROOM_VARIANT_UNDERPASS_SWITCH_SET_ROOM       = 1,
        ROOM_VARIANT_UNDERPASS_SWITCH_CLEAR_ROOM     = 2,
        ROOM_VARIANT_UNDERPASS_ALTERNATE_ROOM_OFFSET = 2,
    };
    *reply = *request;
    if (request->areaId == GAME_AREA_DRYFIELD_WATER_HOLE && request->queryOnly == ROOM_EVENT_EXECUTE) {
        if (gameFlagGetNibble(GAME_FLAG_UNDERPASS_SWITCH_1) == 0) {
            reply->room = ROOM_VARIANT_UNDERPASS_SWITCH_CLEAR_ROOM;
        } else {
            reply->room = ROOM_VARIANT_UNDERPASS_SWITCH_SET_ROOM;
        }
        if (gameFlagGetNibble(GAME_FLAG_053) != 0) {
            reply->room += ROOM_VARIANT_UNDERPASS_ALTERNATE_ROOM_OFFSET;
        }
    }
    if (request->areaId == GAME_AREA_DRYFIELD_CELLAR && request->queryOnly == ROOM_EVENT_EXECUTE) {
        if (gameFlagGetNibble(GAME_FLAG_UNDERPASS_SWITCH_2) == 0) {
            reply->room = ROOM_VARIANT_UNDERPASS_SWITCH_CLEAR_ROOM;
        } else {
            reply->room = ROOM_VARIANT_UNDERPASS_SWITCH_SET_ROOM;
        }
    }
    return ROOM_VARIANT_TRANSITION_DIRECT;
}
