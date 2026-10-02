/* Part of the room variants library; see room_variants.h. */

/// Neo Ark areas. Unless the request is a query it sets `arg1->room`: areas 7,
/// 13 and 32 give nibble 0xE1, 0xD9 or 0xDD plus 1; area 20 gives 1, 2 once
/// 0xDD is set, or 3 once 0xDC is also set; area 21 gives 4 once 0xE9 is set,
/// otherwise 1. Always returns 1.
s32 roomVariantResolveNeoArk(RoomEventMsg* arg0, RoomEventMsg* arg1)
{
    if (arg0->queryOnly == ROOM_EVENT_EXECUTE) {
        switch (arg0->areaId) {
            case GAME_AREA_MINE_FORKED_TUNNEL:
                arg1->room = GameFlag_GetNibble(0xE1) + 1;
                break;
            case GAME_AREA_SHELTER_B1_ARMORY:
                arg1->room = GameFlag_GetNibble(0xD9) + 1;
                break;
            case GAME_AREA_SHELTER_B1_UNDERGROUND_PARKING:
                arg1->room = 1;
                if (GameFlag_GetNibble(0xDD) != 0) {
                    if (GameFlag_GetNibble(0xDC) != 0) {
                        arg1->room = 3;
                    } else {
                        arg1->room = 2;
                    }
                }
                break;
            case GAME_AREA_SHELTER_B1_GOLEM_FREEZER_1:
                if (GameFlag_GetNibble(0xE9) != 0) {
                    arg1->room = 4;
                } else {
                    arg1->room = 1;
                }
                break;
            case GAME_AREA_SHELTER_B2_BREEDING_ROOM:
                arg1->room = GameFlag_GetNibble(0xDD) + 1;
                break;
            case GAME_AREA_MINE_SECRET_PASSAGE:
            case GAME_AREA_SHELTER_B1_ELEVATOR_HALL:
            case GAME_AREA_SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY:
            case GAME_AREA_SHELTER_B1_STOREROOM:
            case GAME_AREA_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY:
            case GAME_AREA_SHELTER_B1_SLEEPING_QUARTERS:
            case GAME_AREA_SHELTER_B1_MAIN_CORRIDOR:
            case GAME_AREA_SHELTER_B1_STERILIZATION_ROOM:
            case GAME_AREA_SHELTER_B1_POD_ACCESS_TUNNEL:
            case GAME_AREA_SHELTER_B1_CONTROL_ROOM:
            case GAME_AREA_SHELTER_B1_ACCESS_TUNNEL:
            case GAME_AREA_SHELTER_B2_POD_BOTTOM:
            case GAME_AREA_SHELTER_B1_POD_SERVICE_GANTRY:
            case GAME_AREA_SHELTER_B1_TRANSFER_TUNNEL:
            case GAME_AREA_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL:
            case GAME_AREA_SHELTER_B2_ELEVATOR:
            case GAME_AREA_SHELTER_B2_ELEVATOR_HALL:
            case GAME_AREA_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY:
            case GAME_AREA_SHELTER_B2_OPERATING_ROOM:
            case GAME_AREA_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY:
            case GAME_AREA_SHELTER_B2_LABORATORY:
            default:
                break;
        }
    }
    return 1;
}
