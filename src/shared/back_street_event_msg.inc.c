/* Part of the back street library; see back_street.h. */

/// Resolves the back street's warehouse variant and locked-house departure.
///
/// Receives a borrowed request and writable reply for ROOM_EVENT_MESSAGE_RESOLVE;
/// they may alias. Queries copy the record but preserve its destination and skip
/// CAP, flag and ambience changes. Execution selects warehouse room 1 or 2 in
/// daytime Dryfield. A locked house runs the stage's refusal command and returns
/// 0 (stay); other departures return 1 and stop the daytime ambience. The task
/// and message ID are ignored; the request and current room resources must be live.
static s32 _roomVariantResolveBackStreet(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        ROOM_VARIANT_BACK_STREET_STAY                = 0,
        ROOM_VARIANT_BACK_STREET_DEPART              = 1,
        ROOM_VARIANT_BACK_STREET_HOUSE_DAY_COMMAND   = 2,
        ROOM_VARIANT_BACK_STREET_HOUSE_NIGHT_COMMAND = 9,
        ROOM_VARIANT_BACK_STREET_EVENT_HANDLED       = 2,
        ROOM_VARIANT_BACK_STREET_AMBIENCE_FADE       = 15,
    };
    u8 stageId;

    *reply  = *request;
    stageId = gGameSession->location.loc.stage;
    if (stageId == GAME_STAGE_DRYFIELD && request->areaId == GAME_AREA_DRYFIELD_WAREHOUSE && request->queryOnly == ROOM_EVENT_EXECUTE) {
        if (gameFlagGetNibble(GAME_FLAG_WAREHOUSE_EVENT_SEEN) == 0) {
            reply->room = 1;
        } else {
            reply->room = stageId;
        }
    }
    if ((request->areaId == GAME_AREA_DRYFIELD_DILAPIDATED_HOUSE) && (gameFlagGetNibble(GAME_FLAG_DILAPIDATED_HOUSE_DOOR_UNLOCKED) == 0)) {
        if (request->queryOnly == ROOM_EVENT_EXECUTE) {
            s32 commandIndex = ROOM_VARIANT_BACK_STREET_HOUSE_NIGHT_COMMAND;

            if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
                commandIndex = ROOM_VARIANT_BACK_STREET_HOUSE_DAY_COMMAND;
            }
            capRunCommandWithTransition(commandIndex);
            gameFlagSetNibbleIfPresent(request->flagId, ROOM_VARIANT_BACK_STREET_EVENT_HANDLED);
        }
        return ROOM_VARIANT_BACK_STREET_STAY;
    }
    if (request->queryOnly == ROOM_EVENT_EXECUTE) {
        if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
            sndEvtRequestScriptStop(SOUND_BACK_STREET_AMBIENCE, ROOM_VARIANT_BACK_STREET_AMBIENCE_FADE);
        }
    }
    return ROOM_VARIANT_BACK_STREET_DEPART;
}
