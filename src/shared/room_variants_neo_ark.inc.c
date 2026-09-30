/* Part of the room variants library; see room_variants.h. */

/// Neo Ark areas. Unless the request is a query it sets `arg1->room`: areas 7,
/// 13 and 32 give nibble 0xE1, 0xD9 or 0xDD plus 1; area 20 gives 1, 2 once
/// 0xDD is set, or 3 once 0xDC is also set; area 21 gives 4 once 0xE9 is set,
/// otherwise 1. Always returns 1.
s32 roomVariantResolveNeoArk(RoomEventMsg* arg0, RoomEventMsg* arg1)
{
    if (arg0->queryOnly == ROOM_EVENT_EXECUTE) {
        switch (arg0->areaId) {
            case 7:
                arg1->room = GameFlag_GetNibble(0xE1) + 1;
                break;
            case 13:
                arg1->room = GameFlag_GetNibble(0xD9) + 1;
                break;
            case 20:
                arg1->room = 1;
                if (GameFlag_GetNibble(0xDD) != 0) {
                    if (GameFlag_GetNibble(0xDC) != 0) {
                        arg1->room = 3;
                    } else {
                        arg1->room = 2;
                    }
                }
                break;
            case 21:
                if (GameFlag_GetNibble(0xE9) != 0) {
                    arg1->room = 4;
                } else {
                    arg1->room = 1;
                }
                break;
            case 32:
                arg1->room = GameFlag_GetNibble(0xDD) + 1;
                break;
            case 8:
            case 9:
            case 10:
            case 11:
            case 12:
            case 14:
            case 15:
            case 16:
            case 17:
            case 18:
            case 19:
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
            default:
                break;
        }
    }
    return 1;
}
