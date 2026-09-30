/* Part of the general store library; see general_store.h. */

/// Handler for the store's two event ids. Both answer with a furniture-style
/// "which variant" byte in `out->room`, and a non-zero `queryOnly` asks what
/// would happen without the side effects.
///
/// Message 1 is the grandfather clock: with nibble 0x63 clear the reply is the
/// id itself, otherwise 4, or 2 + nibble 0x61 while nibble 0x7A is still below
/// 4. The final arm offers the gate a request that plays the two stage sounds
/// 0x5203000C / 0x52030003 under flag nibble 0x3B.
///
/// Message 0x26 is the shop till: with nibble 0xC9 set the reply is 2, or 1
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
        if (GameFlag_GetNibble(0x63) == 0) {
            out->room = msgId;
        } else {
            if (GameFlag_GetNibble(0x7A) >= 4) {
                v = 4;
            } else {
                v = GameFlag_GetNibble(0x61) + 2;
            }
            out->room = v;
        }
    }
    if (in->areaId == 0x26 && in->queryOnly == ROOM_EVENT_EXECUTE) {
        if (GameFlag_GetNibble(0xC9) != 0) {
            if (GameFlag_GetNibble(0x53) == 0) {
                out->room = 1;
            } else {
                out->room = 2;
            }
            if (GameFlag_GetNibble(0x51) == 0) {
                out->room = out->room + 2;
            }
        } else if (GameFlag_GetNibble(0x51) == 0) {
            out->room = 6;
        } else {
            out->room = 5;
        }
    }
    if (in->areaId == 1) {
        req.capCmd        = 0xD;
        req.missingCapCmd = 0xD;
        req.firstSnd      = Gp_PackStageSndId(0x5203000C);
        req.secondSnd     = Gp_PackStageSndId(0x52030003);
        req.flagId        = 0x3B;
        req.collectedBit  = 0;
        return roomEventGate(&req, in);
    }
    if (in->areaId != 0x26) {
        return 1;
    }
    if (in->queryOnly != ROOM_EVENT_EXECUTE) {
        return 2;
    }
    if (GameFlag_GetNibble(0x62) == 0) {
        Task_SpawnFromTable(gStoreTaskDescs, 1, 0, 0);
        gStoreWarp = in->warp;
        gStoreRoom = in->room;
    } else {
        Gp_RunCapCmd1(0xE);
    }
    return 2;
}
