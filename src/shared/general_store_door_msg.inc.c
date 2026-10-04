/* Part of the general store library; see general_store.h. */

/// Handler for transitions to the store's two neighbouring areas, in the day
/// and night stages alike. Both answer with a "which variant" byte in
/// `out->room`, and a non-zero `queryOnly` asks what would happen without the
/// side effects.
///
/// Area 1 is the gas station: with nibble 0x63 clear the reply is the
/// id itself, otherwise 4, or 2 + nibble 0x61 while nibble 0x7A is still below
/// 4. The final arm offers the gate a request that plays the two stage sounds
/// 0x5203000C / 0x52030003 under flag nibble 0x3B.
///
/// Area 0x26 is the underpass: with nibble 0xC9 set the reply is 2, or 1
/// while nibble 0x53 is clear, plus 2 more while nibble 0x51 is clear;
/// otherwise 5, or 6 while nibble 0x51 is clear. The arm that is not asking
/// latches `warp` / `room` for the spawned task and answers 2, or runs
/// CAP command 0xE when nibble 0x62 is set. Anything else answers 1.
s32 storeDoorMsg(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventReq req;
    u16          msgId;
    s32          v;

    *out  = *in;
    msgId = in->areaId;
    if (msgId == 1 && in->queryOnly == ROOM_EVENT_EXECUTE) {
        if (gameFlagGetNibble(GAME_FLAG_NIGHT_GAS_STATION_PROGRESS) == 0) {
            out->room = msgId;
        } else {
            if (gameFlagGetNibble(GAME_FLAG_STORY_CHAPTER) >= 4) {
                v = 4;
            } else {
                v = gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SCENE_SEEN) + 2;
            }
            out->room = v;
        }
    }
    if (in->areaId == 0x26 && in->queryOnly == ROOM_EVENT_EXECUTE) {
        if (gameFlagGetNibble(GAME_FLAG_UNDERPASS_EVENT_SEEN) != 0) {
            if (gameFlagGetNibble(GAME_FLAG_053) == 0) {
                out->room = 1;
            } else {
                out->room = 2;
            }
            if (gameFlagGetNibble(GAME_FLAG_UNDERPASS_SWITCH_1) == 0) {
                out->room = out->room + 2;
            }
        } else if (gameFlagGetNibble(GAME_FLAG_UNDERPASS_SWITCH_1) == 0) {
            out->room = 6;
        } else {
            out->room = 5;
        }
    }
    if (in->areaId == 1) {
        req.capCmd        = 0xD;
        req.missingCapCmd = 0xD;
        req.firstSnd      = Gp_PackStageSndId(SOUND_GENERAL_STORE_DOOR_UNLOCK);
        req.secondSnd     = Gp_PackStageSndId(SOUND_GENERAL_STORE_DOOR_OPEN);
        req.flagId        = GAME_FLAG_GENERAL_STORE_DOOR_UNLOCKED;
        req.collectedBit  = 0;
        return roomEventGate(&req, in);
    }
    if (in->areaId != 0x26) {
        return 1;
    }
    if (in->queryOnly != ROOM_EVENT_EXECUTE) {
        return 2;
    }
    if (gameFlagGetNibble(GAME_FLAG_GENERAL_STORE_UNDERPASS_BLOCKED) == 0) {
        Task_SpawnFromTable(gStoreTaskDescs, 1, 0, 0);
        gStoreWarp = in->warp;
        gStoreRoom = in->room;
    } else {
        Gp_RunCapCmd1(0xE);
    }
    return 2;
}
