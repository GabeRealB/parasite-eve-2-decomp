/* Part of the dryfield driveway library; see dryfield_driveway.h. */

/// Resolves driveway destinations and substitutes progress-dependent scenes for departure.
///
/// ROOM_EVENT_MESSAGE_RESOLVE supplies a borrowed request and writable reply,
/// which may alias. Queries preserve the destination and skip CAP, task and game
/// flag updates; the factory gate still clears the staged-event indicator.
/// Execution resolves factory/water-hole
/// room variants, may block departure with CAP or the encounter task, and latches
/// the one-shot factory transition scene. Returns 0 to stay, 1 for normal departure
/// or 2 for a room replacement action. CAP/script resources and the room event task
/// bank must be live; the receiver and message ID are ignored.
static s32 _roomVariantResolveDriveway(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        ROOM_VARIANT_DRIVEWAY_STAY                = 0,
        ROOM_VARIANT_DRIVEWAY_DEPART              = 1,
        ROOM_VARIANT_DRIVEWAY_ACTION              = 2,
        ROOM_VARIANT_DRIVEWAY_CAP_BALCONY         = 6,
        ROOM_VARIANT_DRIVEWAY_CAP_WATER_HOLE      = 1,
        ROOM_VARIANT_DRIVEWAY_CAP_FACTORY_BLOCKED = 2,
        ROOM_VARIANT_DRIVEWAY_CAP_FACTORY_SCENE   = 9,
        ROOM_VARIANT_DRIVEWAY_FACTORY_SCENE_SOUND = 0x52190003,
        ROOM_VARIANT_DRIVEWAY_CUTSCENE_INDEX      = 1,
        ROOM_VARIANT_DRIVEWAY_EVENT_HANDLED       = 2,
        ROOM_VARIANT_DRIVEWAY_PROGRESS_READY      = 2,
    };
    RoomLatchedEvent  event;
    RoomLatchedEvent* eventRequest;
    s32               flagClear;

    *reply = *request;
    if (request->areaId == GAME_AREA_DRYFIELD_FACTORY && request->queryOnly == ROOM_EVENT_EXECUTE) {
        flagClear   = gameFlagGetNibble(GAME_FLAG_FACTORY_BARRIER_CLEARED) == 0;
        reply->room = flagClear ? 1 : 2;
    }
    if (request->areaId == GAME_AREA_DRYFIELD_WATER_HOLE && request->queryOnly == ROOM_EVENT_EXECUTE) {
        flagClear   = gameFlagGetNibble(GAME_FLAG_UNDERPASS_SWITCH_1) == 0;
        reply->room = flagClear ? 2 : 1;
        if (gameFlagGetNibble(GAME_FLAG_053) != 0) {
            reply->room = reply->room + 2;
        }
    }
    if (request->areaId == GAME_AREA_DRYFIELD_MAIN_STREET && gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SCENE_SEEN) != 0) {
        if (request->queryOnly == ROOM_EVENT_EXECUTE) {
            capRunCommandWithTransition(ROOM_VARIANT_DRIVEWAY_CAP_BALCONY);
            gameFlagSetNibbleIfPresent(request->flagId, ROOM_VARIANT_DRIVEWAY_EVENT_HANDLED);
        }
        return ROOM_VARIANT_DRIVEWAY_ACTION;
    }
    if (request->areaId == GAME_AREA_DRYFIELD_WATER_HOLE) {
        if (gameFlagGetNibble(GAME_FLAG_DRIVEWAY_PROGRESS) != ROOM_VARIANT_DRIVEWAY_PROGRESS_READY) {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
                    if (gGameSession->location.loc.variant == 1) {
                        if (gameFlagGetNibble(GAME_FLAG_050) == 0) {
                            taskSpawnFromTable(gDrivewayCutsceneTasks, ROOM_VARIANT_DRIVEWAY_CUTSCENE_INDEX, 0, 0);
                            return ROOM_VARIANT_DRIVEWAY_STAY;
                        }
                    }
                }
                if (gGameSession->location.loc.variant == 1 && gSceneCombatState.signals.bytes.battlePhase == gGameSession->location.loc.variant) {
                    return ROOM_VARIANT_DRIVEWAY_STAY;
                }
                capRunCommandWithTransition(ROOM_VARIANT_DRIVEWAY_CAP_WATER_HOLE);
                return ROOM_VARIANT_DRIVEWAY_STAY;
            }
            return ROOM_VARIANT_DRIVEWAY_STAY;
        }
        if (request->queryOnly == ROOM_EVENT_EXECUTE && gameFlagGetNibble(GAME_FLAG_COMPANION_2_SCHEDULE) == 1) {
            gameFlagSetNibble(GAME_FLAG_COMPANION_2_SCHEDULE, 2);
        }
    }
    if (request->areaId == GAME_AREA_DRYFIELD_FACTORY) {
        if (gameFlagGetNibble(GAME_FLAG_030) == 1) {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                gameFlagSetNibbleIfPresent(request->flagId, ROOM_VARIANT_DRIVEWAY_EVENT_HANDLED);
                capRunCommandWithTransition(ROOM_VARIANT_DRIVEWAY_CAP_FACTORY_BLOCKED);
                return ROOM_VARIANT_DRIVEWAY_ACTION;
            }
            return ROOM_VARIANT_DRIVEWAY_ACTION;
        }
        // Retain complete values for the staged event before this stack record expires.
        event.capCmd   = ROOM_VARIANT_DRIVEWAY_CAP_FACTORY_SCENE;
        event.stageSnd = ROOM_VARIANT_DRIVEWAY_FACTORY_SCENE_SOUND;
        event.flagId   = GAME_FLAG_DRIVEWAY_TO_FACTORY_SCENE;
        event.fade     = 0;
        eventRequest   = &event;
        // The factory gate clears this indicator for queries as well as execution.
        gDrivewayEventSpawned = 0;
        if (gameFlagGetNibble(eventRequest->flagId) == 0 || eventRequest->flagId == 0) {
            if (reply->queryOnly == ROOM_EVENT_EXECUTE) {
                gRoomEventStagedMsg = *reply;
                gRoomEventLatched   = event;
                if (eventRequest->flagId != 0) {
                    gameFlagSetNibble(eventRequest->flagId, 1);
                }
                taskSpawnFromTable(&gRoomEventStagedTaskDesc, 0, 0, 0);
                gDrivewayEventSpawned = 1;
                return ROOM_VARIANT_DRIVEWAY_ACTION;
            }
            return ROOM_VARIANT_DRIVEWAY_ACTION;
        }
    }
    return ROOM_VARIANT_DRIVEWAY_DEPART;
}
