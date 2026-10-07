/* Part of the room variants library; see room_variants.h. */

// Each carrier binds this identifier to its RoomVariantResolver definition.
#ifndef ROOM_VARIANT_RESOLVE_NEO_ARK
#error Define ROOM_VARIANT_RESOLVE_NEO_ARK before including this fragment.
#endif

enum {
    ROOM_VARIANT_NEO_ARK_DEFAULT_ROOM               = 1,
    ROOM_VARIANT_NEO_ARK_ALTAR_SECOND_SEQUENCE_ROOM = 2,
    ROOM_VARIANT_NEO_ARK_ALTAR_BOTH_SEQUENCES_ROOM  = 3,
    ROOM_VARIANT_NEO_ARK_SHRINE_ALTERNATE_ROOM      = 4,
};

/// Selects the altar room reached by the completed tile sequences.
static __inline__ void _roomVariantNeoArkResolveAltar(RoomEventMsg* reply)
{
    reply->room = ROOM_VARIANT_NEO_ARK_DEFAULT_ROOM;
    if (gameFlagGetNibble(GAME_FLAG_NEO_ARK_ALTAR_SEQUENCE_2_SOLVED) != 0) {
        if (gameFlagGetNibble(GAME_FLAG_NEO_ARK_ALTAR_SEQUENCE_1_SOLVED) != 0) {
            reply->room = ROOM_VARIANT_NEO_ARK_ALTAR_BOTH_SEQUENCES_ROOM;
        } else {
            reply->room = ROOM_VARIANT_NEO_ARK_ALTAR_SECOND_SEQUENCE_ROOM;
        }
    }
}

s32 ROOM_VARIANT_RESOLVE_NEO_ARK(RoomEventMsg* request, RoomEventMsg* reply)
{
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
    return 1;
}
