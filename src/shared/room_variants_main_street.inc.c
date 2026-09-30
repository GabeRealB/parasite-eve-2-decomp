/* Part of the room variants library; see room_variants.h. */

/// Message-table handler for id 0x13EE: echoes the incoming record into the
/// reply and, for a message 2 that is not report-only (`queryOnly == 0`),
/// answers game nibble 0x61 plus one while game nibble 0x7A is below 4, and 3
/// once it has reached 4. Returns 1.
s32 roomVariantMainStreetMsg(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    s32 val;
    s32 n;

    *out = *in;
    if (in->areaId == 2 && in->queryOnly == ROOM_EVENT_EXECUTE) {
        n = GameFlag_GetNibble(0x7A);
        if (n >= 4) {
            val = 3;
        } else {
            val = GameFlag_GetNibble(0x61) + 1;
        }
        out->room = val;
    }
    return 1;
}
