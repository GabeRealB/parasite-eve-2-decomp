/* Part of the water tower library; see water_tower.h. */

/// The room's handler for message 0x13EE, the first entry of its message table.
/// It copies the incoming record to `out` and answers by the record's first
/// halfword. For 0x13 it builds the room's event request -- flag nibble 0x34,
/// collected bit 0x10, CAP commands 0xA and 6 and two stage sounds -- and
/// hands it to the event gate with the incoming record; the gate's 0 (the
/// prerequisite missing) is answered as 2, and once the gate has latched the
/// event item 0x110 is marked seen. Any other record first drops nibble 0x55
/// from 2 back to 1 unless it is only a query. For 0x15 it also clears nibble
/// 0x4B when it reads 7, and answers 1 on stage 3 and otherwise only while
/// nibble 0x32 is 2. Everything else answers 1.
s32 waterTowerEventMsg(Task* task, s32 msgId, RoomEventMsg* msg, RoomEventMsg* out)
{
    RoomEventReq req;
    s32          ret;

    *out = *msg;
    if (msg->areaId == 0x13) {
        req.capCmd        = 0xA;
        req.missingCapCmd = 6;
        req.firstSnd      = Gp_PackStageSndId(SOUND_WATER_TOWER_KITCHEN_DOOR_UNLOCK);
        req.secondSnd     = Gp_PackStageSndId(SOUND_WATER_TOWER_KITCHEN_DOOR_OPEN);
        req.flagId        = GAME_FLAG_KITCHEN_WATER_TOWER_DOOR_UNLOCKED;
        req.collectedBit  = 0x10;
        ret               = roomEventGate(&req, msg);
        if (ret == 0) {
            ret = 2;
        }
        if (ROOM_EVENT_ACTIVE != 0) {
            Gp_SetItemSeenBit(0x110, 1);
        }
        return ret;
    }
    if (msg->queryOnly == ROOM_EVENT_EXECUTE && gameFlagGetNibble(GAME_FLAG_WATER_TOWER_MECHANISM_STATE) == 2) {
        gameFlagSetNibble(GAME_FLAG_WATER_TOWER_MECHANISM_STATE, 1);
    }
    if (msg->areaId == 0x15) {
        if (msg->queryOnly == ROOM_EVENT_EXECUTE && gameFlagGetNibble(GAME_FLAG_COMPANION_2_SCHEDULE) == 7) {
            gameFlagSetNibble(GAME_FLAG_COMPANION_2_SCHEDULE, 0);
        }
        if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD_NIGHT) {
            return 1;
        }
        if (gameFlagGetNibble(GAME_FLAG_WATER_TOWER_PROGRESS) != 2) {
            return 0;
        }
    }
    return 1;
}
