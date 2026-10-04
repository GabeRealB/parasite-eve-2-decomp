/* Part of the room variants library; see room_variants.h. */

/// Neo Ark areas. Unless the request is a query it sets `arg1->room`: areas 7,
/// 13 and 32 give nibble 0xE1, 0xD9 or 0xDD plus 1; area 20 gives 1, 2 once
/// 0xDD is set, or 3 once 0xDC is also set; area 21 gives 4 once 0xE9 is set,
/// otherwise 1. Always returns 1.
s32 roomVariantResolveNeoArk(RoomEventMsg* arg0, RoomEventMsg* arg1)
{
    if (arg0->queryOnly == ROOM_EVENT_EXECUTE) {
        switch (arg0->areaId) {
            case GAME_AREA_NEO_ARK_OBSERVATORY:
                arg1->room = gameFlagGetNibble(GAME_FLAG_0E1) + 1;
                break;
            case GAME_AREA_NEO_ARK_PAVILION:
                arg1->room = gameFlagGetNibble(GAME_FLAG_NEO_ARK_ALTAR_SWITCH_STATE) + 1;
                break;
            case GAME_AREA_NEO_ARK_ALTAR:
                arg1->room = 1;
                if (gameFlagGetNibble(GAME_FLAG_NEO_ARK_ALTAR_SEQUENCE_2_SOLVED) != 0) {
                    if (gameFlagGetNibble(GAME_FLAG_NEO_ARK_ALTAR_SEQUENCE_1_SOLVED) != 0) {
                        arg1->room = 3;
                    } else {
                        arg1->room = 2;
                    }
                }
                break;
            case GAME_AREA_NEO_ARK_SHRINE:
                if (gameFlagGetNibble(GAME_FLAG_0E9) != 0) {
                    arg1->room = 4;
                } else {
                    arg1->room = 1;
                }
                break;
            case GAME_AREA_NEO_ARK_PYRAMID:
                arg1->room = gameFlagGetNibble(GAME_FLAG_NEO_ARK_ALTAR_SEQUENCE_2_SOLVED) + 1;
                break;
            case GAME_AREA_NEO_ARK_EVE_ACCESS_TUNNEL:
            case GAME_AREA_NEO_ARK_EVE_ELEVATOR:
            case GAME_AREA_NEO_ARK_NORTH_PROMENADE:
            case GAME_AREA_NEO_ARK_FOREST_ZONE:
            case GAME_AREA_NEO_ARK_SUBMARINE_TUNNEL:
            case GAME_AREA_NEO_ARK_ISLAND:
            case GAME_AREA_NEO_ARK_GARDEN:
            case GAME_AREA_NEO_ARK_POWER_PLANT_2:
            case GAME_AREA_NEO_ARK_POWER_PLANT_1:
            case GAME_AREA_NEO_ARK_SAVANNA_ZONE:
            case GAME_AREA_NEO_ARK_SOUTH_PROMENADE:
            case GAME_AREA_SHELTER_B6_NURSERY:
            case GAME_AREA_SHELTER_B6_GROWTH_ROOM:
            case GAME_AREA_SHELTER_B6_CORRIDOR:
            case GAME_AREA_SHELTER_B6_TRAINING_ROOM:
            case GAME_AREA_NEO_ARK_R26:
            case GAME_AREA_NEO_ARK_BRIDGE:
            case GAME_AREA_SHELTER_1F_TENT:
            case GAME_AREA_NEO_ARK_WOODLAND_PATH:
            case GAME_AREA_NEO_ARK_SUBMARINE_GALLERY:
            case GAME_AREA_NEO_ARK_R31:
            default:
                break;
        }
    }
    return 1;
}
