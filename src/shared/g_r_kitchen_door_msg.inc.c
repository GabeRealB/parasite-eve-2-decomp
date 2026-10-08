/* Part of the G & R kitchen library; see g_r_kitchen.h. */

/// Resolves the G & R kitchen's gated departure to the water tower.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE`; borrows complete eight-byte request
/// and reply records, which may alias, and copies the request first. The water
/// tower door uses the room event gate: flag 0x34 bypasses it when set, otherwise
/// CAP command 3 and stage-relative unlock/open sounds manage the departure.
/// Queries suppress event playback. Returns the gate's 0/1/2 result, or 1 for
/// other destinations. Retains no input pointer; an event copies the records
/// and requires the room resources to remain loaded until its task completes.
static s32 _roomVariantGRKitchenMsg(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        G_R_KITCHEN_WATER_TOWER_CAP_COMMAND  = 3,
        G_R_KITCHEN_WATER_TOWER_UNLOCK_SOUND = DRYFIELD_STAGE_SOUND((GAME_AREA_DRYFIELD_G_R_KITCHEN << 16) | 1),
        G_R_KITCHEN_WATER_TOWER_OPEN_SOUND   = DRYFIELD_STAGE_SOUND((GAME_AREA_DRYFIELD_G_R_KITCHEN << 16) | 4),
    };
    RoomEventReq eventRequest;
    s32          transitionResult;

    *reply = *request;
    if (request->areaId == GAME_AREA_DRYFIELD_WATER_TOWER) {
        eventRequest.capCmd        = G_R_KITCHEN_WATER_TOWER_CAP_COMMAND;
        eventRequest.missingCapCmd = G_R_KITCHEN_WATER_TOWER_CAP_COMMAND;
        eventRequest.firstSnd      = G_R_KITCHEN_WATER_TOWER_UNLOCK_SOUND;
        eventRequest.secondSnd     = G_R_KITCHEN_WATER_TOWER_OPEN_SOUND;
        eventRequest.flagId        = GAME_FLAG_KITCHEN_WATER_TOWER_DOOR_UNLOCKED;
        eventRequest.collectedBit  = ROOM_EVENT_GATE_NO_COLLECTION_REQUIRED;
        transitionResult           = _roomEventGate(&eventRequest, request);
    } else {
        transitionResult = ROOM_VARIANT_TRANSITION_DIRECT;
    }
    return transitionResult;
}
