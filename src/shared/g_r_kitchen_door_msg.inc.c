/* Part of the G & R kitchen library; see g_r_kitchen.h. */

/// Handler for message 0x13EE in the room's message table, which filters a
/// warp request: copies `in` to `out`, and for area 0x14 passes the warp
/// through the event gate with the room's own request - nibble 0x34, no collected bit,
/// cap command 3 and the stage bank's sounds 0x130001 and 0x130004 - answering
/// with the gate's result. Any other area answers 1.
s32 grKitchenDoorMsg(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventReq req;
    s32          ret;

    *out = *in;
    if (in->areaId == 0x14) {
        req.capCmd        = 3;
        req.missingCapCmd = 3;
        req.firstSnd      = DRYFIELD_STAGE_SOUND(0x130001);
        req.secondSnd     = DRYFIELD_STAGE_SOUND(0x130004);
        req.flagId        = GAME_FLAG_KITCHEN_WATER_TOWER_DOOR_UNLOCKED;
        req.collectedBit  = 0;
        ret               = _roomEventGate(&req, in);
    } else {
        ret = 1;
    }
    return ret;
}
