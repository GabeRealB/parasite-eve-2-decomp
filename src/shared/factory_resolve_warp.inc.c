/* Part of the factory lift library; see factory_lift.h. */

/// Filters a warp request: copies `in` to `out`, choosing the destination room
/// for area 0x19 from the stage variant and progress flags, and for area 0x18
/// from game flag 0x7A. Area 0x18 is refused with cap slot 4 until game flag
/// 0x4A reaches 2, area 0x16 with cap command 0xD while game flag 0x37 is
/// clear, and area 0x19 goes through the event gate with the room's own
/// request. Any other warp answers 1.
s32 factoryResolveWarp(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventReq req;
    u8           variant;

    *out = *in;
    if (in->areaId == 0x19) {
        variant = gGameSession->location.loc.stage;
        if (variant == 2) {
            if (in->queryOnly == ROOM_EVENT_EXECUTE) {
                if (GameFlag_GetNibble(0x3A) >= 2) {
                    out->room = variant;
                } else {
                    out->room = 1;
                }
            }
        } else if (in->queryOnly == ROOM_EVENT_EXECUTE) {
            out->room = GameFlag_GetNibble(0x61) + 1;
        }
    }
    if (in->areaId == 0x18) {
        if (in->queryOnly == ROOM_EVENT_EXECUTE) {
            if (GameFlag_GetNibble(0x7A) < 4) {
                out->room = 1;
            } else {
                out->room = 2;
            }
        }
        if (in->areaId == 0x18) {
            if (GameFlag_GetNibble(0x4A) != 2) {
                if (in->queryOnly != ROOM_EVENT_EXECUTE) {
                    return 0;
                }
                Gp_StartCapSlot(4, 1, 0);
                Gp_SetNibbleIf(in->flagId, 2);
                return 0;
            }
        }
    }
    if (in->areaId == 0x16) {
        if (GameFlag_GetNibble(0x37) == 0) {
            if (in->queryOnly != ROOM_EVENT_EXECUTE) {
                return 0;
            }
            Gp_SetNibbleIf(in->flagId, 2);
            Gp_RunCapCmd1(0xD);
            return 0;
        }
    }
    if (in->areaId == 0x19) {
        req.capCmd        = 0xE;
        req.missingCapCmd = 0xE;
        req.firstSnd      = 0x52170013;
        req.secondSnd     = 0x52170003;
        req.flagId        = -0x30;
        req.collectedBit  = 0;
        return roomEventGate(&req, in);
    }
    return 1;
}
