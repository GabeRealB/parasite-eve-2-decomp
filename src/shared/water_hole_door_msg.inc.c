/* Part of the water hole library; see water_hole.h. */

/// Handler for message 0x13EE in the room's message table. It copies the
/// incoming record to `out` and, unless `in->queryOnly` is set, answers two
/// queries in `out->room`:
///
/// - 0x19: while the session's stage is 2, 2 once progress nibble 0x3A has
///   reached 2 and 1 before; in any other stage, nibble 0x61 plus one.
/// - 0x26: with nibble 0xC9 set, 2 or 1 by nibble 0x53, plus 2 while nibble
///   0x51 is clear; with 0xC9 clear, 5 or 6 by whether nibble 0x51 is set.
///
/// Always returns 1.
s32 waterHoleDoorMsg(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    u8 temp;

    *out = *in;
    if (in->areaId == 0x19) {
        temp = gGameSession->location.loc.stage;
        if (temp == 2) {
            if (in->queryOnly == ROOM_EVENT_EXECUTE) {
                if (gameFlagGetNibble(GAME_FLAG_DRIVEWAY_PROGRESS) >= 2) {
                    out->room = temp;
                } else {
                    out->room = 1;
                }
            }
        } else if (in->queryOnly == ROOM_EVENT_EXECUTE) {
            out->room = gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SCENE_SEEN) + 1;
        }
    }
    if (in->areaId == 0x26 && in->queryOnly == ROOM_EVENT_EXECUTE) {
        if (gameFlagGetNibble(GAME_FLAG_UNDERPASS_EVENT_SEEN) != 0) {
            if (gameFlagGetNibble(GAME_FLAG_053) != 0) {
                out->room = 2;
            } else {
                out->room = 1;
            }
            if (gameFlagGetNibble(GAME_FLAG_UNDERPASS_SWITCH_1) == 0) {
                out->room += 2;
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
