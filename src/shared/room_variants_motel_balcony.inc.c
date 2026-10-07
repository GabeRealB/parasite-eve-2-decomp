/* Part of the room variants library; see room_variants.h. */

/// Selects the carrier-declared s32 (Task*, s32, const RoomEventMsg*, RoomEventMsg*) handler.
///
/// Night motel room 5/6 use the private default. The night loft binds its
/// overlay-private `roomVariantMotelBalconyMsg` around this fragment.
/// The replacement is one function identifier, without argument evaluation.
#ifndef ROOM_VARIANT_MOTEL_BALCONY_MSG
#define ROOM_VARIANT_MOTEL_BALCONY_MSG _roomVariantMotelBalconyMsg

/// Resolves the night balcony's room after its story scene.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE`; `task` and `messageId` are unused.
/// Borrows a complete request and writable reply, which may alias. Copies the
/// request; execution selects room 1 before the balcony scene and room 3 after
/// it. Queries retain the copied room. Returns 1 for every destination.
#endif
s32 ROOM_VARIANT_MOTEL_BALCONY_MSG(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        ROOM_VARIANT_MOTEL_BALCONY_BEFORE_SCENE_ROOM = 1,
        ROOM_VARIANT_MOTEL_BALCONY_AFTER_SCENE_ROOM  = 3,
    };
    s32 destinationRoom;

    /// Resolves the balcony scene flag into a destination room selector.
    ///
    /// Requires a side-effect-free s32 lvalue, evaluated three times. Captures
    /// this function's two room constants; control stays within the block.
#define ROOM_VARIANT_SELECT_MOTEL_BALCONY_ROOM(roomSelector)                          \
    {                                                                                 \
        (roomSelector) = gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SCENE_SEEN); \
        if ((roomSelector) == 0) {                                                    \
            (roomSelector) = ROOM_VARIANT_MOTEL_BALCONY_BEFORE_SCENE_ROOM;            \
        } else {                                                                      \
            (roomSelector) = ROOM_VARIANT_MOTEL_BALCONY_AFTER_SCENE_ROOM;             \
        }                                                                             \
    }

    *reply = *request;
    if (request->areaId == GAME_AREA_DRYFIELD_NIGHT_MOTEL_BALCONY && request->queryOnly == ROOM_EVENT_EXECUTE) {
        ROOM_VARIANT_SELECT_MOTEL_BALCONY_ROOM(destinationRoom);
        reply->room = destinationRoom;
    }
    return ROOM_VARIANT_TRANSITION_DIRECT;
#undef ROOM_VARIANT_SELECT_MOTEL_BALCONY_ROOM
}
#undef ROOM_VARIANT_MOTEL_BALCONY_MSG
