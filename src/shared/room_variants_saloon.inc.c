/* Part of the room variants library; see room_variants.h. */

/// Handler for message 0x13EE in the room's message table, which filters a
/// warp request: copies `in` to `out`, and for area 0xF picks the destination
/// room from game-flag nibble 0x61 (unless `in->queryOnly` asks for a dry run),
/// then passes the warp through the event gate with the room's own request -
/// nibble 0x35, no collected bit, cap command 2 and two stage sound ids. Any other area
/// answers 1.
s32 roomVariantSaloonMsg(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventReq req;
    u16          msgId;

    *out  = *in;
    msgId = in->areaId;
    if (msgId == 0xF) {
        if (in->queryOnly == ROOM_EVENT_EXECUTE) {
            out->room = GameFlag_GetNibble(0x61) + 1;
        }
        if (in->areaId == msgId) {
            req.capCmd        = 2;
            req.missingCapCmd = 2;
            req.firstSnd      = Gp_PackStageSndId(0x52120005);
            req.secondSnd     = Gp_PackStageSndId(0x52120003);
            req.flagId        = 0x35;
            req.collectedBit  = 0;
            return roomEventGate(&req, in);
        }
    }
    return 1;
}
