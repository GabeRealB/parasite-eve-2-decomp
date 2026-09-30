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
            case 2:
                if (GameFlag_GetNibble(0x10F) != 0) {
                    arg1->room = 2;
                }
                if (GameFlag_GetNibble(0x11A) >= 2) {
                    arg1->room = 3;
                }
                break;
            case 5:
                arg1->room = GameFlag_GetNibble(0xA4) + 1;
                break;
            case 16:
                if (GameFlag_GetNibble(0x7A) >= 6) {
                    arg1->room = 3;
                }
                break;
            case 20:
                switch (GameFlag_GetNibble(0xF4)) {
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
            case 45:
                arg1->room = GameFlag_GetNibble(0xB7) + 1;
                break;
            case 41:
                arg1->room = GameFlag_GetNibble(0xB6) + 1;
                break;
            case 3:
            case 4:
            case 6:
            case 7:
            case 8:
            case 9:
            case 10:
            case 11:
            case 12:
            case 13:
            case 14:
            case 15:
            case 17:
            case 18:
            case 19:
            case 21:
            case 22:
            case 23:
            case 24:
            case 25:
            case 26:
            case 27:
            case 28:
            case 29:
            case 30:
            case 31:
            case 32:
            case 33:
            case 34:
            case 35:
            case 36:
            case 37:
            case 38:
            case 39:
            case 40:
            case 42:
            case 43:
            case 44:
            default:
                break;
        }
    }
    return 1;
}
