/* Part of the room variants library; see room_variants.h. */

// The carrier supplies a function identifier with the RoomVariantResolver
// signature and declares it in its prologue to establish its linkage.
#ifndef ROOM_VARIANT_RESOLVE_NEO_ARK
#error Define ROOM_VARIANT_RESOLVE_NEO_ARK before including this fragment.
#endif

enum {
    ROOM_VARIANT_NEO_ARK_DEFAULT_ROOM = 1,
};

/// Selects the altar's room from its two tile-sequence completion flags.
///
/// Only `reply->room` is written: 1 until sequence 2 is solved, 2 when only
/// sequence 2 is solved, or 3 when both are solved. Sequence 1 alone keeps
/// room 1. The writable record is borrowed for this call and is not retained.
static __inline__ void _roomVariantNeoArkResolveAltar(RoomEventMsg* reply)
{
    enum {
        ROOM_VARIANT_NEO_ARK_ALTAR_SECOND_SEQUENCE_ROOM = 2,
        ROOM_VARIANT_NEO_ARK_ALTAR_BOTH_SEQUENCES_ROOM  = 3,
    };

    reply->room = ROOM_VARIANT_NEO_ARK_DEFAULT_ROOM;
    if (gameFlagGetNibble(GAME_FLAG_NEO_ARK_ALTAR_SEQUENCE_2_SOLVED) != 0) {
        if (gameFlagGetNibble(GAME_FLAG_NEO_ARK_ALTAR_SEQUENCE_1_SOLVED) != 0) {
            reply->room = ROOM_VARIANT_NEO_ARK_ALTAR_BOTH_SEQUENCES_ROOM;
        } else {
            reply->room = ROOM_VARIANT_NEO_ARK_ALTAR_SECOND_SEQUENCE_ROOM;
        }
    }
}

/// Resolves Neo Ark destination rooms from game progress.
///
/// Reads `request->queryOnly` and, on `ROOM_EVENT_EXECUTE`, the stage-5
/// `request->areaId`. Only `reply->room` can change: observatory and pavilion
/// use their progress nibble plus 1, altar uses the tile sequences, shrine
/// selects 1 or 4, and pyramid uses sequence 2's completion nibble plus 1.
/// Queries and other areas preserve the reply; initialize its room before
/// calling. Direct nibble-to-room rules yield 1..16, and progress values must
/// select a room present in the destination area's tables.
///
/// Both pointers borrow live RoomEventMsg records and may alias. Neither is
/// retained; game flags are only read. Always returns 1, without testing
/// passage gates. Each room-local instance is static; the map exports its copy.
s32 ROOM_VARIANT_RESOLVE_NEO_ARK(RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        ROOM_VARIANT_NEO_ARK_SHRINE_ALTERNATE_ROOM = 4,
        ROOM_VARIANT_NEO_ARK_ACCEPTED              = 1,
    };

    // Resolve only the room selector; callers preserve the rest of the record.
    if (request->queryOnly == ROOM_EVENT_EXECUTE) {
        switch (request->areaId) {
            case GAME_AREA_NEO_ARK_OBSERVATORY:
                reply->room = gameFlagGetNibble(GAME_FLAG_0E1) + 1;
                break;
            case GAME_AREA_NEO_ARK_PAVILION:
                reply->room = gameFlagGetNibble(GAME_FLAG_NEO_ARK_ALTAR_SWITCH_STATE) + 1;
                break;
            case GAME_AREA_NEO_ARK_ALTAR:
                _roomVariantNeoArkResolveAltar(reply);
                break;
            case GAME_AREA_NEO_ARK_SHRINE:
                if (gameFlagGetNibble(GAME_FLAG_0E9) != 0) {
                    reply->room = ROOM_VARIANT_NEO_ARK_SHRINE_ALTERNATE_ROOM;
                } else {
                    reply->room = ROOM_VARIANT_NEO_ARK_DEFAULT_ROOM;
                }
                break;
            case GAME_AREA_NEO_ARK_PYRAMID:
                reply->room = gameFlagGetNibble(GAME_FLAG_NEO_ARK_ALTAR_SEQUENCE_2_SOLVED) + 1;
                break;
            default:
                break;
        }
    }
    return ROOM_VARIANT_NEO_ARK_ACCEPTED;
}
