/* Part of the room variants library; see room_variants.h. */

/// Resolves the gas station's main-street departure and checks its two exits.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE`; `task` and `messageId` are unused.
/// Borrows a complete request and writable reply, which may be the same record.
/// Copies the request before resolving main street's room on execution only.
/// Returns 0 for a locked general-store exit or blocked main-street exit, 2
/// for the combat-controlled general-store exit in night variant 1, otherwise 1.
/// Queries suppress CAP commands and the optional refusal-flag write.
static s32 _roomVariantGasStationMsg(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        ROOM_VARIANT_GAS_STATION_BATTLE_VARIANT           = 1,
        ROOM_VARIANT_GAS_STATION_MAIN_STREET_BLOCKED      = 1,
        ROOM_VARIANT_GAS_STATION_CAP_COMBAT_EXIT          = 0x15,
        ROOM_VARIANT_GAS_STATION_CAP_GENERAL_STORE_LOCKED = 7,
        ROOM_VARIANT_GAS_STATION_CAP_MAIN_STREET_BLOCKED  = 8,
        ROOM_VARIANT_GAS_STATION_REFUSAL_FLAG_VALUE       = 2,
    };
    s32 storyChapter;
    s32 destinationRoom;

    *reply = *request;
    if (request->areaId == GAME_AREA_DRYFIELD_MAIN_STREET && request->queryOnly == ROOM_EVENT_EXECUTE) {
        storyChapter = gameFlagGetNibble(GAME_FLAG_STORY_CHAPTER);
        if (storyChapter >= ROOM_VARIANT_DRYFIELD_FINAL_CHAPTER) {
            destinationRoom = ROOM_VARIANT_MAIN_STREET_FINAL_ROOM;
        } else {
            destinationRoom = gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SCENE_SEEN) + 1;
        }
        reply->room = destinationRoom;
    }
    // In night variant 1, combat claims the general-store exit before its lock test.
    if (request->areaId == GAME_AREA_DRYFIELD_GENERAL_STORE) {
        if ((gGameSession->location.loc.stage == request->areaId) && (gGameSession->location.loc.variant == ROOM_VARIANT_GAS_STATION_BATTLE_VARIANT) &&
            (gSceneCombatState.signals.bytes.battlePhase == gGameSession->location.loc.variant)) {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                capRunCommandWithTransition(ROOM_VARIANT_GAS_STATION_CAP_COMBAT_EXIT);
            }
            return ROOM_VARIANT_TRANSITION_HANDLED;
        }
        if (gameFlagGetNibble(GAME_FLAG_GENERAL_STORE_DOOR_UNLOCKED) == 0) {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                capRunCommandWithTransition(ROOM_VARIANT_GAS_STATION_CAP_GENERAL_STORE_LOCKED);
                gameFlagSetNibbleIfPresent(request->flagId, ROOM_VARIANT_GAS_STATION_REFUSAL_FLAG_VALUE);
            }
            return ROOM_VARIANT_TRANSITION_REFUSED;
        }
    }
    if (request->areaId == GAME_AREA_DRYFIELD_MAIN_STREET) {
        if (gameFlagGetNibble(GAME_FLAG_GAS_STATION_MAIN_STREET_BLOCKED) == ROOM_VARIANT_GAS_STATION_MAIN_STREET_BLOCKED) {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                capRunCommandWithTransition(ROOM_VARIANT_GAS_STATION_CAP_MAIN_STREET_BLOCKED);
            }
            return ROOM_VARIANT_TRANSITION_REFUSED;
        }
    }
    return ROOM_VARIANT_TRANSITION_DIRECT;
}
