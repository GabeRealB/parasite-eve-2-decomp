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
    if (in->areaId == GAME_AREA_DRYFIELD_NIGHT_MAIN_STREET && in->queryOnly == ROOM_EVENT_EXECUTE) {
        n = gameFlagGetNibble(GAME_FLAG_STORY_CHAPTER);
        if (n >= 4) {
            val = 3;
        } else {
            val = gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SCENE_SEEN) + 1;
        }
        out->room = val;
    }
    return 1;
}
