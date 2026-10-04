/* Part of the room variants library; see room_variants.h. */

/// Handler for message 0x13EE: copies the incoming record onto the outgoing one
/// and, unless the query is report-only (`queryOnly` set), answers record id 0x20
/// with 1 or 2 from nibble 0x51, raised by 2 while nibble 0x53 is set, and
/// record id 0x22 with 1 or 2 from nibble 0x52. Always returns 1.
s32 roomVariantUnderpassMsg(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    if (in->areaId == 0x20 && in->queryOnly == ROOM_EVENT_EXECUTE) {
        if (gameFlagGetNibble(GAME_FLAG_UNDERPASS_SWITCH_1) == 0) {
            out->room = 2;
        } else {
            out->room = 1;
        }
        if (gameFlagGetNibble(GAME_FLAG_053) != 0) {
            out->room += 2;
        }
    }
    if (in->areaId == 0x22 && in->queryOnly == ROOM_EVENT_EXECUTE) {
        if (gameFlagGetNibble(GAME_FLAG_UNDERPASS_SWITCH_2) == 0) {
            out->room = 2;
        } else {
            out->room = 1;
        }
    }
    return 1;
}
