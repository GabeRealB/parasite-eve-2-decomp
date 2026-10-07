/* Part of the room variants library; see room_variants.h. */

/// The room's message handler. It copies `msg` to `out`, filling `room`
/// from game flags for messages 0x1C, 0xF and 0x1F, then routes messages 0x1C,
/// 0x1F and 0x1E through the event gate with each one's request; when the
/// gate fires, it updates collection and item identification flags (and, for 0x1E, a
/// flag nibble and `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent`). Any other message answers 1; a gate result
/// of 0 is reported as 2.
s32 roomVariantMotelBalconyDoorsMsg(Task* task, s32 msgId, RoomEventMsg* msg, RoomEventMsg* out)
{
    RoomEventReq req;
    s32          flagClear;
    s32          ret;

    *out = *msg;
    if (msg->areaId == 0x1C && msg->queryOnly == ROOM_EVENT_EXECUTE) {
        out->room = gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SCENE_SEEN) + 1;
    }
    if (msg->areaId == 0xF && msg->queryOnly == ROOM_EVENT_EXECUTE) {
        out->room = gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SCENE_SEEN) + 1;
    }
    if (msg->areaId == 0x1F && msg->queryOnly == ROOM_EVENT_EXECUTE) {
        flagClear = gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_LOFT_EVENT_SEEN) == 0;
        out->room = flagClear ? 1 : 2;
    }
    if (msg->areaId == 0x1C) {
        req.capCmd        = 7;
        req.missingCapCmd = 4;
        req.firstSnd      = Gp_PackStageSndId(SOUND_MOTEL_BALCONY_DOOR_UNLOCK);
        req.secondSnd     = Gp_PackStageSndId(SOUND_MOTEL_BALCONY_DOOR_OPEN);
        req.flagId        = GAME_FLAG_MOTEL_ROOM_5_DOOR_UNLOCKED;
        req.collectedBit  = 0x13;
        ret               = roomEventGate(&req, out);
        if (ROOM_EVENT_ACTIVE != 0) {
            inventoryClearCollectedBit(INVENTORY_COLLECTION_ID_MOTEL_ROOM_6_KEY);
            inventoryClearCollectedBit(INVENTORY_COLLECTION_ID_LOBBY_KEY);
            itemSetIdentified(0x113, 1);
        }
    } else if (msg->areaId == 0x1F) {
        req.capCmd        = 5;
        req.missingCapCmd = 2;
        req.firstSnd      = Gp_PackStageSndId(SOUND_MOTEL_BALCONY_DOOR_UNLOCK);
        req.secondSnd     = Gp_PackStageSndId(SOUND_MOTEL_BALCONY_DOOR_OPEN);
        req.flagId        = GAME_FLAG_MOTEL_LOFT_DOOR_UNLOCKED;
        req.collectedBit  = 0x13;
        ret               = roomEventGate(&req, out);
        if (ROOM_EVENT_ACTIVE != 0) {
            inventoryClearCollectedBit(INVENTORY_COLLECTION_ID_MOTEL_ROOM_6_KEY);
            inventoryClearCollectedBit(INVENTORY_COLLECTION_ID_LOBBY_KEY);
            itemSetIdentified(0x113, 1);
        }
    } else if (msg->areaId == 0x1E) {
        req.capCmd        = 6;
        req.missingCapCmd = 3;
        req.firstSnd      = Gp_PackStageSndId(SOUND_MOTEL_BALCONY_DOOR_UNLOCK);
        req.secondSnd     = Gp_PackStageSndId(SOUND_MOTEL_BALCONY_DOOR_OPEN);
        req.flagId        = GAME_FLAG_MOTEL_ROOM_6_DOOR_UNLOCKED;
        req.collectedBit  = 0xF;
        ret               = roomEventGate(&req, out);
        if (ROOM_EVENT_ACTIVE != 0) {
            gameFlagSetNibble(GAME_FLAG_030, 1);
            gMcSaveData[0].state.sceneEvent = 3;
            func_800E3FAC(0xA2, 0xC);
        }
    } else {
        return 1;
    }
    if (ret == 0) {
        ret = 2;
    }
    return ret;
}
