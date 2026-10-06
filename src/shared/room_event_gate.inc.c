/* Part of the room events library; see room_events.h. */

/// The room's event gate, called by the room's message handler with the
/// request it builds on the stack. A set flag nibble (or a clear one, for a
/// negative `flagId`) means the event has already happened and the answer is
/// 1; a missing collected-bit prerequisite runs the request's `missingCapCmd`
/// and answers 0; otherwise the request and message are latched into
/// `gRoomEventReq` / `gRoomEventMsg`, the flag
/// nibble is written, the event task is spawned and
/// `gRoomEventActive` is raised, for 2. A non-zero `queryOnly` on
/// the message asks what would happen and suppresses all of those effects.
s32 roomEventGate(RoomEventReq* req, RoomEventMsg* msg)
{
    s32 flag;
    s32 id;
    s32 mode;
    s32 got;
    s32 ret;
    s32 neg;

    flag              = req->flagId;
    ROOM_EVENT_ACTIVE = 0;
    neg               = flag < 0;
    got               = (s16)flag;
    if (neg) {
        flag = -flag;
        got  = gameFlagGetNibble(flag) == 0;
    } else {
        got = gameFlagGetNibble(got);
    }
    ret = 1;
    if (got == 0) {
        if (inventoryHasCollectedBit(req->collectedBit) != 0 || req->collectedBit == 0) {
            ret = 2;
            if (msg->queryOnly == ROOM_EVENT_EXECUTE) {
                gRoomEventMsg  = *msg;
                ROOM_EVENT_REQ = *req;
                id             = req->flagId;
                mode           = 1;
                if (id < 0) {
                    id   = -id;
                    mode = 0;
                }
                gameFlagSetNibble(id, mode);
                taskSpawnFromTable(&gRoomEventTaskDesc, 0, 0, 0);
                ROOM_EVENT_ACTIVE = 1;
                return 2;
            }
            return ret;
        }
        ret = 0;
        if (msg->queryOnly == ROOM_EVENT_EXECUTE) {
            Gp_RunCapCmd1(req->missingCapCmd);
            Gp_SetNibbleIf(msg->flagId, 2);
            ret = 0;
        }
        return ret;
    }
    return ret;
}
