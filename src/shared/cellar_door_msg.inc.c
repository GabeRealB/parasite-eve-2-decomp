/* Part of the cellar library; see cellar.h. */

/// Resolves the cellar's underpass destination from saved event and switch state.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE` in both cellar rooms. Borrows a readable
/// request and writable reply, which may alias, and copies the complete record.
/// Executing an underpass transition selects room 1..6; queries and other areas
/// preserve the requested room. Always returns 1; receiver and ID are unused.
static s32 _roomVariantResolveCellar(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        CELLAR_UNDERPASS_ROOM_AFTER_EVENT             = 1,
        CELLAR_UNDERPASS_ROOM_AFTER_EVENT_FLAG_053    = 2,
        CELLAR_UNDERPASS_SWITCH_OFF_ROOM_OFFSET       = 2,
        CELLAR_UNDERPASS_ROOM_BEFORE_EVENT_SWITCH_ON  = 5,
        CELLAR_UNDERPASS_ROOM_BEFORE_EVENT_SWITCH_OFF = 6,
    };
    *reply = *request;
    if (request->areaId == GAME_AREA_DRYFIELD_UNDERPASS && request->queryOnly == ROOM_EVENT_EXECUTE) {
        if (gameFlagGetNibble(GAME_FLAG_UNDERPASS_EVENT_SEEN) != 0) {
            if (gameFlagGetNibble(GAME_FLAG_053) != 0) {
                reply->room = CELLAR_UNDERPASS_ROOM_AFTER_EVENT_FLAG_053;
            } else {
                reply->room = CELLAR_UNDERPASS_ROOM_AFTER_EVENT;
            }
            if (gameFlagGetNibble(GAME_FLAG_UNDERPASS_SWITCH_1) == 0) {
                reply->room = (u8)reply->room + CELLAR_UNDERPASS_SWITCH_OFF_ROOM_OFFSET;
            }
        } else {
            if (gameFlagGetNibble(GAME_FLAG_UNDERPASS_SWITCH_1) != 0) {
                reply->room = CELLAR_UNDERPASS_ROOM_BEFORE_EVENT_SWITCH_ON;
            } else {
                reply->room = CELLAR_UNDERPASS_ROOM_BEFORE_EVENT_SWITCH_OFF;
            }
        }
    }
    return 1;
}
