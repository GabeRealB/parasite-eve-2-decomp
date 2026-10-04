/* Part of the room variants library; see room_variants.h. */

/// Shelter areas. Unless the request is a query it sets `arg1->room` from
/// progress nibbles: area 2 gives 2 once 0x10F is set and 3 once 0x11A reaches
/// 2; areas 5, 41 and 45 give nibble 0xA4, 0xB6 or 0xB7 plus 1; 16 gives 3 once
/// 0x7A reaches 6; 20 maps nibble 0xF4's values 0-3 to 1, 6, 7 and 8 (1
/// otherwise). Other areas are left untouched. Always returns 1.
s32 roomVariantResolveShelter(RoomEventMsg* arg0, RoomEventMsg* arg1)
{
    if (arg0->queryOnly == ROOM_EVENT_EXECUTE) {
        switch (arg0->areaId) {
            case GAME_AREA_MINE_CAVERN:
                if (gameFlagGetNibble(GAME_FLAG_MINE_CAVERN_INTRO_SEEN) != 0) {
                    arg1->room = 2;
                }
                if (gameFlagGetNibble(GAME_FLAG_MINE_SECRET_PASSAGE_PROGRESS) >= 2) {
                    arg1->room = 3;
                }
                break;
            case GAME_AREA_MINE_GORGE:
                arg1->room = gameFlagGetNibble(GAME_FLAG_MINE_GORGE_TRIGGER_EVENT_DONE) + 1;
                break;
            case GAME_AREA_SHELTER_B1_STERILIZATION_ROOM:
                if (gameFlagGetNibble(GAME_FLAG_STORY_CHAPTER) >= 6) {
                    arg1->room = 3;
                }
                break;
            case GAME_AREA_SHELTER_B1_UNDERGROUND_PARKING:
                switch (gameFlagGetNibble(GAME_FLAG_UNDERGROUND_PARKING_STATE)) {
                    case 0:
                        arg1->room = 1;
                        break;
                    case 1:
                        arg1->room = 6;
                        break;
                    case 2:
                        arg1->room = 7;
                        break;
                    case 3:
                        arg1->room = 8;
                        break;
                    default:
                        arg1->room = 1;
                        break;
                }
                break;
            case GAME_AREA_SHELTER_B4_RESERVOIR:
                arg1->room = gameFlagGetNibble(GAME_FLAG_B4_RESERVOIR_EVENT_DONE) + 1;
                break;
            case GAME_AREA_SHELTER_B3_INCINERATOR_CONTROL_ROOM:
                arg1->room = gameFlagGetNibble(GAME_FLAG_INCINERATOR_CONTROL_ROOM_STATE) + 1;
                break;
            case GAME_AREA_MINE_TUNNEL_ENTRANCE:
            case GAME_AREA_MINE_TUNNEL:
            case GAME_AREA_MINE_REFUGE:
            case GAME_AREA_MINE_FORKED_TUNNEL:
            case GAME_AREA_MINE_SECRET_PASSAGE:
            case GAME_AREA_SHELTER_B1_ELEVATOR_HALL:
            case GAME_AREA_SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY:
            case GAME_AREA_SHELTER_B1_STOREROOM:
            case GAME_AREA_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY:
            case GAME_AREA_SHELTER_B1_ARMORY:
            case GAME_AREA_SHELTER_B1_SLEEPING_QUARTERS:
            case GAME_AREA_SHELTER_B1_MAIN_CORRIDOR:
            case GAME_AREA_SHELTER_B1_POD_ACCESS_TUNNEL:
            case GAME_AREA_SHELTER_B1_CONTROL_ROOM:
            case GAME_AREA_SHELTER_B1_ACCESS_TUNNEL:
            case GAME_AREA_SHELTER_B1_GOLEM_FREEZER_1:
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
            case GAME_AREA_SHELTER_B2_BREEDING_ROOM:
            case GAME_AREA_SHELTER_B2_MAIN_CORRIDOR:
            case GAME_AREA_SHELTER_B2_SEPTIC_TANK:
            case GAME_AREA_SHELTER_B2_POD_ACCESS_TUNNEL:
            case GAME_AREA_SHELTER_R36:
            case GAME_AREA_SHELTER_R37:
            case GAME_AREA_SHELTER_1F_HELIPORT_S4:
            case GAME_AREA_SHELTER_B3_DUMPING_HOLE:
            case GAME_AREA_SHELTER_B3_GARBAGE_INCINERATOR:
            case GAME_AREA_SHELTER_B3_ELEVATOR_HALL:
            case GAME_AREA_SHELTER_B4_LOWER_SEWER:
            case GAME_AREA_SHELTER_B4_UPPER_SEWER:
            default:
                break;
        }
    }
    return 1;
}
