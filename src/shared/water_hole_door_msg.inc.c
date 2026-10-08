/* Part of the water hole library; see water_hole.h. */

/// Resolves the water hole's driveway and underpass exits from game progress.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE` using a borrowed eight-byte request and
/// writable reply, which may be the same record. Copies the request first;
/// queries and other destinations preserve it. By day the driveway selects
/// room 1 or 2 at progress 2; otherwise it selects the balcony-scene nibble + 1.
/// After the underpass event, flag 0x53 selects room 1/2 and a clear switch 1
/// adds 2. Before that event, switch 1 selects room 5 (set) or 6 (clear).
///
/// Always permits the ordinary departure. Only the copied reply's room changes.
static s32 _roomVariantResolveWaterHole(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        ROOM_VARIANT_WATER_HOLE_DRIVEWAY_PROGRESS_READY             = 2,
        ROOM_VARIANT_WATER_HOLE_DEFAULT_ROOM                        = 1,
        ROOM_VARIANT_WATER_HOLE_DRIVEWAY_OPEN_ROOM                  = 2,
        ROOM_VARIANT_WATER_HOLE_UNDERPASS_EVENT_ROOM                = 1,
        ROOM_VARIANT_WATER_HOLE_UNDERPASS_FLAG_053_ROOM             = 2,
        ROOM_VARIANT_WATER_HOLE_UNDERPASS_SWITCH_OFF_ROOM_OFFSET    = 2,
        ROOM_VARIANT_WATER_HOLE_UNDERPASS_PRE_EVENT_SWITCH_ON_ROOM  = 5,
        ROOM_VARIANT_WATER_HOLE_UNDERPASS_PRE_EVENT_SWITCH_OFF_ROOM = 6,
    };
    u8 currentStage;

    *reply = *request;
    if (request->areaId == GAME_AREA_DRYFIELD_DRIVEWAY) {
        currentStage = gGameSession->location.loc.stage;
        if (currentStage == GAME_STAGE_DRYFIELD) {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                if (gameFlagGetNibble(GAME_FLAG_DRIVEWAY_PROGRESS) >= ROOM_VARIANT_WATER_HOLE_DRIVEWAY_PROGRESS_READY) {
                    reply->room = ROOM_VARIANT_WATER_HOLE_DRIVEWAY_OPEN_ROOM;
                } else {
                    reply->room = ROOM_VARIANT_WATER_HOLE_DEFAULT_ROOM;
                }
            }
        } else if (request->queryOnly == ROOM_EVENT_EXECUTE) {
            reply->room = gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SCENE_SEEN) + ROOM_VARIANT_WATER_HOLE_DEFAULT_ROOM;
        }
    }
    if (request->areaId == GAME_AREA_DRYFIELD_UNDERPASS && request->queryOnly == ROOM_EVENT_EXECUTE) {
        if (gameFlagGetNibble(GAME_FLAG_UNDERPASS_EVENT_SEEN) != 0) {
            if (gameFlagGetNibble(GAME_FLAG_053) != 0) {
                reply->room = ROOM_VARIANT_WATER_HOLE_UNDERPASS_FLAG_053_ROOM;
            } else {
                reply->room = ROOM_VARIANT_WATER_HOLE_UNDERPASS_EVENT_ROOM;
            }
            if (gameFlagGetNibble(GAME_FLAG_UNDERPASS_SWITCH_1) == 0) {
                reply->room += ROOM_VARIANT_WATER_HOLE_UNDERPASS_SWITCH_OFF_ROOM_OFFSET;
            }
        } else {
            if (gameFlagGetNibble(GAME_FLAG_UNDERPASS_SWITCH_1) != 0) {
                reply->room = ROOM_VARIANT_WATER_HOLE_UNDERPASS_PRE_EVENT_SWITCH_ON_ROOM;
            } else {
                reply->room = ROOM_VARIANT_WATER_HOLE_UNDERPASS_PRE_EVENT_SWITCH_OFF_ROOM;
            }
        }
    }
    return ROOM_VARIANT_TRANSITION_DIRECT;
}
