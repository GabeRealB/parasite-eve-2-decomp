/* Part of the room variants library; see room_variants.h. */

/// Message-table handler for id 0x13EE: echoes the incoming record into the
/// reply and, for a message 0x1D that is not report-only (`queryOnly == 0`),
/// answers 1 while game nibble 0x61 is clear and 3 once it is set. Returns 1.
s32 roomVariantMotelBalconyMsg(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    s32 nib;

    *out = *in;
    if (in->areaId == GAME_AREA_DRYFIELD_NIGHT_MOTEL_BALCONY && in->queryOnly == ROOM_EVENT_EXECUTE) {
        nib = gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SCENE_SEEN);
        if (nib == 0) {
            nib = 1;
        } else {
            nib = 3;
        }
        out->room = nib;
    }
    return 1;
}
