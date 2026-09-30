/* Part of the room variants library; see room_variants.h. */

/// Message-table handler for id 0x13EE: echoes the incoming record into the
/// reply and, for a message 0x1D that is not report-only (`queryOnly == 0`),
/// answers 1 while game nibble 0x61 is clear and 3 once it is set. Returns 1.
s32 roomVariantMotelBalconyMsg(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    s32 nib;

    *out = *in;
    if (in->areaId == 0x1D && in->queryOnly == ROOM_EVENT_EXECUTE) {
        nib = GameFlag_GetNibble(0x61);
        if (nib == 0) {
            nib = 1;
        } else {
            nib = 3;
        }
        out->room = nib;
    }
    return 1;
}
