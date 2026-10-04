/* Part of the room variants library; see room_variants.h. */

/// Message-table handler for id 0x13EE: echoes the incoming record into the
/// reply and, for a message 0xF that is not report-only (`queryOnly == 0`),
/// answers game nibble 0x61 plus one. Returns 1.
s32 roomVariantParkingLotMsg(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    if (in->areaId == 0xF && in->queryOnly == ROOM_EVENT_EXECUTE) {
        out->room = gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SCENE_SEEN) + 1;
    }
    return 1;
}
