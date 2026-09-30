/* Part of the room variants library; see room_variants.h. */

/// The room's message handler. It copies `msg` to `out`, filling `room`
/// from game flags for messages 0x1C, 0xF and 0x1F, then routes messages 0x1C,
/// 0x1F and 0x1E through the event gate with each one's request; when the
/// gate fires, it updates the collected and seen item bits (and, for 0x1E, a
/// flag nibble and `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent`). Any other message answers 1; a gate result
/// of 0 is reported as 2.
s32 roomVariantMotelBalconyDoorsMsg(Task* task, s32 msgId, RoomEventMsg* msg, RoomEventMsg* out)
{
    RoomEventReq req;
    s32          flagClear;
    s32          ret;

    *out = *msg;
    if (msg->areaId == 0x1C && msg->queryOnly == ROOM_EVENT_EXECUTE) {
        out->room = GameFlag_GetNibble(0x61) + 1;
    }
    if (msg->areaId == 0xF && msg->queryOnly == ROOM_EVENT_EXECUTE) {
        out->room = GameFlag_GetNibble(0x61) + 1;
    }
    if (msg->areaId == 0x1F && msg->queryOnly == ROOM_EVENT_EXECUTE) {
        flagClear = GameFlag_GetNibble(0x96) == 0;
        out->room = flagClear ? 1 : 2;
    }
    if (msg->areaId == 0x1C) {
        req.capCmd        = 7;
        req.missingCapCmd = 4;
        req.firstSnd      = Gp_PackStageSndId(0x521D000A);
        req.secondSnd     = Gp_PackStageSndId(0x521D0001);
        req.flagId        = 0x43;
        req.collectedBit  = 0x13;
        ret               = roomEventGate(&req, out);
        if (ROOM_EVENT_ACTIVE != 0) {
            Gp_ClearCollectedBit(0x10F);
            Gp_ClearCollectedBit(0x112);
            Gp_SetItemSeenBit(0x113, 1);
        }
    } else if (msg->areaId == 0x1F) {
        req.capCmd        = 5;
        req.missingCapCmd = 2;
        req.firstSnd      = Gp_PackStageSndId(0x521D000A);
        req.secondSnd     = Gp_PackStageSndId(0x521D0001);
        req.flagId        = 0x44;
        req.collectedBit  = 0x13;
        ret               = roomEventGate(&req, out);
        if (ROOM_EVENT_ACTIVE != 0) {
            Gp_ClearCollectedBit(0x10F);
            Gp_ClearCollectedBit(0x112);
            Gp_SetItemSeenBit(0x113, 1);
        }
    } else if (msg->areaId == 0x1E) {
        req.capCmd        = 6;
        req.missingCapCmd = 3;
        req.firstSnd      = Gp_PackStageSndId(0x521D000A);
        req.secondSnd     = Gp_PackStageSndId(0x521D0001);
        req.flagId        = 0x2E;
        req.collectedBit  = 0xF;
        ret               = roomEventGate(&req, out);
        if (ROOM_EVENT_ACTIVE != 0) {
            GameFlag_SetNibble(0x30, 1);
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
