/* Part of the room variants library; see room_variants.h. */

#ifndef ROOM_VARIANT_RESOLVE_SHELTER
#error Include room_variants.h and retain its resolver binding through this fragment.
#endif

/// Selects the underground parking layout reached by its event sequence.
static __inline__ void _roomVariantShelterResolveParking(RoomEventMsg* reply)
{
    enum {
        ROOM_VARIANT_DEFAULT_ROOM                        = 1,
        ROOM_VARIANT_PARKING_STATE_INITIAL               = 0,
        ROOM_VARIANT_PARKING_STATE_AFTER_CHOICE          = 1,
        ROOM_VARIANT_PARKING_STATE_SCENE_STARTED         = 2,
        ROOM_VARIANT_PARKING_STATE_STERILIZATION_ENTERED = 3,
        ROOM_VARIANT_PARKING_ROOM_AFTER_CHOICE           = 6,
        ROOM_VARIANT_PARKING_ROOM_SCENE_STARTED          = 7,
        ROOM_VARIANT_PARKING_ROOM_STERILIZATION_ENTERED  = 8
    };

    switch (gameFlagGetNibble(GAME_FLAG_UNDERGROUND_PARKING_STATE)) {
        case ROOM_VARIANT_PARKING_STATE_INITIAL:
            reply->room = ROOM_VARIANT_DEFAULT_ROOM;
            break;
        case ROOM_VARIANT_PARKING_STATE_AFTER_CHOICE:
            reply->room = ROOM_VARIANT_PARKING_ROOM_AFTER_CHOICE;
            break;
        case ROOM_VARIANT_PARKING_STATE_SCENE_STARTED:
            reply->room = ROOM_VARIANT_PARKING_ROOM_SCENE_STARTED;
            break;
        case ROOM_VARIANT_PARKING_STATE_STERILIZATION_ENTERED:
            reply->room = ROOM_VARIANT_PARKING_ROOM_STERILIZATION_ENTERED;
            break;
        default:
            reply->room = ROOM_VARIANT_DEFAULT_ROOM;
            break;
    }
}

/// Resolves Mine/Shelter destination rooms from game progress.
///
/// Reads `request->queryOnly` and, on `ROOM_EVENT_EXECUTE`, `request->areaId`
/// within the Mine/Shelter stage. Only `reply->room` can change.
/// Queries, unhandled areas and unmet progress thresholds preserve the reply;
/// initialize its room before calling. Both pointers borrow live RoomEventMsg
/// records for this call and may alias; neither pointer is retained.
/// Always returns 1. Direct nibble-to-room rules yield 1..16; progress values
/// and initialized destinations must identify valid rooms in their areas.
s32 ROOM_VARIANT_RESOLVE_SHELTER(RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        ROOM_VARIANT_MINE_CAVERN_AFTER_INTRO   = 2,
        ROOM_VARIANT_MINE_CAVERN_AFTER_PASSAGE = 3,
        ROOM_VARIANT_MINE_PASSAGE_TRAVERSED    = 2,
        ROOM_VARIANT_BULWARK_STORY_CHAPTER     = 6,
        ROOM_VARIANT_STERILIZATION_LATE_ROOM   = 3,
        ROOM_VARIANT_ACCEPTED                  = 1
    };

    // Preserve the prepared destination except for progress-dependent rooms.
    if (request->queryOnly == ROOM_EVENT_EXECUTE) {
        switch (request->areaId) {
            case GAME_AREA_MINE_CAVERN:
                if (gameFlagGetNibble(GAME_FLAG_MINE_CAVERN_INTRO_SEEN) != 0) {
                    reply->room = ROOM_VARIANT_MINE_CAVERN_AFTER_INTRO;
                }
                // Traversing the secret passage supersedes the cavern's intro room.
                if (gameFlagGetNibble(GAME_FLAG_MINE_SECRET_PASSAGE_PROGRESS) >= ROOM_VARIANT_MINE_PASSAGE_TRAVERSED) {
                    reply->room = ROOM_VARIANT_MINE_CAVERN_AFTER_PASSAGE;
                }
                break;
            case GAME_AREA_MINE_GORGE:
                reply->room = gameFlagGetNibble(GAME_FLAG_MINE_GORGE_TRIGGER_EVENT_DONE) + 1;
                break;
            case GAME_AREA_SHELTER_B1_STERILIZATION_ROOM:
                if (gameFlagGetNibble(GAME_FLAG_STORY_CHAPTER) >= ROOM_VARIANT_BULWARK_STORY_CHAPTER) {
                    reply->room = ROOM_VARIANT_STERILIZATION_LATE_ROOM;
                }
                break;
            case GAME_AREA_SHELTER_B1_UNDERGROUND_PARKING:
                _roomVariantShelterResolveParking(reply);
                break;
            case GAME_AREA_SHELTER_B4_RESERVOIR:
                reply->room = gameFlagGetNibble(GAME_FLAG_B4_RESERVOIR_EVENT_DONE) + 1;
                break;
            case GAME_AREA_SHELTER_B3_INCINERATOR_CONTROL_ROOM:
                reply->room = gameFlagGetNibble(GAME_FLAG_INCINERATOR_CONTROL_ROOM_STATE) + 1;
                break;
            default:
                break;
        }
    }
    return ROOM_VARIANT_ACCEPTED;
}
