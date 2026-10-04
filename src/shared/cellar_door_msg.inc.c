/* Part of the cellar library; see cellar.h. */

/// Message-table handler for message 0x13EE. Copies the incoming record onto
/// the outgoing one; for a query 0x26 without `queryOnly` set it answers in
/// `room` from event nibbles 0xC9, 0x53 and 0x51 (1 to 4 while 0xC9 is set,
/// 5 or 6 otherwise). Always answers 1.
s32 cellarDoorMsg(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    if (in->areaId == 0x26 && in->queryOnly == ROOM_EVENT_EXECUTE) {
        if (gameFlagGetNibble(GAME_FLAG_UNDERPASS_EVENT_SEEN) != 0) {
            if (gameFlagGetNibble(GAME_FLAG_053) != 0) {
                out->room = 2;
            } else {
                out->room = 1;
            }
            if (gameFlagGetNibble(GAME_FLAG_UNDERPASS_SWITCH_1) == 0) {
                out->room = (u8)out->room + 2;
            }
        } else {
            if (gameFlagGetNibble(GAME_FLAG_UNDERPASS_SWITCH_1) != 0) {
                out->room = 5;
            } else {
                out->room = 6;
            }
        }
    }
    return 1;
}
