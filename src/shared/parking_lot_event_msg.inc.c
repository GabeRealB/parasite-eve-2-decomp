/* Part of the parking lot library; see parking_lot.h. */

/// Handler for message 0x13EE in the room's message table: the room's two
/// reports and its two events. The incoming record is first copied to `out`.
///
/// Messages 2 and 0x1D answer in `out->room` (only when `queryOnly` is clear):
/// message 2 gives nibble 0x61 plus one while nibble 0x7A is under 4, and 3
/// once it is not; message 0x1D gives 1 while nibble 0x61 is clear and 3 once
/// it is set.
///
/// Messages 0x11 and 0x12 are the events: each builds a request for the
/// room's event gate `roomEventGate` - message 0x11 on
/// nibble 0x40 with collected bit 0x12, message 0x12 on nibble 0x35 with collected bit 0x10.
/// When the gate reports the event fired, 0x11 applies the area records
/// `gParkingLotAreaRecs` and sets nibbles 0x46 and 0x97, while 0x12 sets item-seen bit
/// 0x110. Any other message returns 1.
s32 parkingLotEventMsg(Task* task, s32 msgId, RoomEventMsg* msg, RoomEventMsg* out)
{
    RoomEventReq req;
    s32          ret;
    s32          val;
    s32          n;

    *out = *msg;
    if ((msg->areaId == 2) && (msg->queryOnly == ROOM_EVENT_EXECUTE)) {
        n = GameFlag_GetNibble(0x7A);
        if (n >= 4) {
            val = 3;
        } else {
            val = GameFlag_GetNibble(0x61) + 1;
        }
        out->room = val;
    }
    if ((msg->areaId == 0x1D) && (msg->queryOnly == ROOM_EVENT_EXECUTE)) {
        n = GameFlag_GetNibble(0x61);
        if (n == 0) {
            n = 1;
        } else {
            n = 3;
        }
        out->room = n;
    }
    if (msg->areaId == 0x11) {
        req.capCmd        = 6;
        req.missingCapCmd = 1;
        req.firstSnd      = Gp_PackStageSndId(0x520F000B);
        req.secondSnd     = Gp_PackStageSndId(0x520F0007);
        req.flagId        = 0x40;
        req.collectedBit  = 0x12;
        ret               = roomEventGate(&req, out);
        if (ret == 0) {
            ret = 2;
        }
        if (ROOM_EVENT_ACTIVE != 0) {
            Gp_ApplyAreaRecs(gParkingLotAreaRecs);
            GameFlag_SetNibble(0x46, 1);
            GameFlag_SetNibble(0x97, 1);
        }
    } else if (msg->areaId == 0x12) {
        req.capCmd        = 3;
        req.missingCapCmd = 2;
        req.firstSnd      = Gp_PackStageSndId(0x520F000B);
        req.secondSnd     = Gp_PackStageSndId(0x520F0007);
        req.flagId        = 0x35;
        req.collectedBit  = 0x10;
        ret               = roomEventGate(&req, out);
        if (ROOM_EVENT_ACTIVE != 0) {
            Gp_SetItemSeenBit(0x110, 1);
        }
    } else {
        return 1;
    }
    return ret;
}
